//#include <netinet/in.h>
//#include "gfxlib.h"
#include "networking/netbuffer.h"
#include "universe_generic.h"
#include "universe_util.h"
//#include "universe_util_generic.h" //Use universe_util_generic.h instead
#include "star_system_generic.h"
#include "cmd/unit_generic.h"
#include "gfx/cockpit_generic.h"
#include "packet.h"
#include "savenet_util.h"
//#include "netserver.h"
#include "zonemgr.h"
#include "vs_globals.h"
#include "endianness.h"
#include <assert.h>

ZoneMgr::ZoneMgr()
{
}

StarSystem *	ZoneMgr::addZone( string starsys)
{
	cout<<">>> ADDING A NEW ZONE = "<<starsys<<" - # OF ZONES = "<<_Universe->star_system.size()<<endl;
	list<Client *> lst;
	StarSystem * sts=NULL;
	// Generate the StarSystem
	string starsysfile = starsys+".system";
	sts = _Universe->GenerateStarSystem (starsysfile.c_str(),"",Vector(0,0,0));
	// Add it in the star_system vector
	//_Universe->star_system.push_back( sts);
	//_Universe->pushActiveStarSystem( sts);
	// Add an empty list of clients to the zone_list vector
	zone_list.push_back( lst);
	// Add zero as number of clients in zone since we increment in ZoneMgr::addClient()
	zone_clients.push_back( 0);
	cout<<"<<< NEW ZONE ADDED - # OF ZONES = "<<_Universe->star_system.size()<<endl;
	return sts;
}

// Return the client list that are in the zone n� serial
list<Client *>	ZoneMgr::GetZone( int serial)
{
	return zone_list[serial];
}

// Adds a client to the zone n� serial
/*
void	ZoneMgr::addClient( Client * clt, int zone)
{
	zone_list[zone].push_back( clt);
	zone_clients[zone]++;
	clt->zone = zone;
	// Now we add the unit in that starsystem
	sts->AddUnit( clt->game_unit);
}
*/

void	ZoneMgr::addUnit( Unit * un, int zone)
{
	zone_unitlist[zone].push_back( un);
	zone_units[zone]++;
}

void	ZoneMgr::removeUnit( Unit * un, int zone)
{
	if( zone_unitlist[zone].empty())
	{
		cout<<"Trying to remove on an empty list !!"<<endl;
		exit( 1);
	}
	zone_unitlist[zone].remove( un);
	zone_units[zone]--;
}

// Returns NULL if no corresponding Unit was found
Unit *	ZoneMgr::getUnit( ObjSerial unserial, int zone)
{
	LUI i;
	Unit * un = NULL;
	for( i=zone_unitlist[zone].begin(); i!=zone_unitlist[zone].end(); i++)
	{
		if( (*i)->GetSerial()==unserial)
			un = (*i);
	}

	return un;
}

bool	ZoneMgr::addClient( Client * clt, string starsys)
{
	// Remove the client from old starsystem if needed and add it in the new one
	/*
	string oldstarsys = clt->save.GetOldStarSystem();
	*/
	bool ret = true;
	StarSystem * sts;
	//Cockpit * cp = _Universe->isPlayerStarship( clt->game_unit.GetUnit());
	//string starsys = cp->savegame->GetStarSystem();
	// TO BE DONE IN JUMP HANDLING !!!
	/*
	if( starsys!=oldstarsys)
	{
		// Remove the player from the old starsystem
		sts = _Universe->getStarSystem( oldstarsys);
		sts->RemoveUnit( clt->game_unit);

		// SOMEDAY TEST IF THE STARSYSTEM WE WANT TO GO IN IS REACHABLE FROM THE OLD ONE
	}
	*/
	if( !(sts = _Universe->getStarSystem( starsys+".system")))
	{
		// Add a network zone (StarSystem equivalent) and create the new StarSystem
		// StarSystem is not loaded so we generate it
		cout<<"--== STAR SYSTEM NOT FOUND - GENERATING ==--"<<endl;
		sts = this->addZone( starsys);
		// It also mean that there is nobody in that system so no need to send update
		// Return false since the starsystem didn't contain any client
		ret = false;
	}
	// Get the index of the star_system as it represents the zone number
	//int zone = _Universe->StarSystemIndex( sts);
	// This way should be more efficient since the system we just added is the star_system.size()-1
	int zone = _Universe->star_system.size()-1;
	cout<<">> ADDING CLIENT IN ZONE # "<<zone<<endl;
	// Adds the client in the zone
	zone_list[zone].push_back( clt);
	clt->zone = zone;
	zone_clients[zone]++;

	// Compute a safe entrance point -> DONE WHEN LOGIN ACCEPTED
	//QVector safevec;
	//safevec = UniverseUtil::SafeEntrancePoint( clt->current_state.getPosition());
	//clt->current_state.setPosition( safevec);
	sts->AddUnit( clt->game_unit.GetUnit());
	return ret;
}

// Remove a client from its zone
void	ZoneMgr::removeClient( Client * clt)
{
	StarSystem * sts;
	Unit * un = clt->game_unit.GetUnit();
	if( zone_list[clt->zone].empty())
	{
		cout<<"Trying to remove on an empty list !!"<<endl;
		exit( 1);
	}

	zone_list[clt->zone].remove( clt);
	zone_clients[clt->zone]--;
	sts = _Universe->star_system[clt->zone];
	sts->RemoveUnit( un);
	// SHIP MAY NOT HAVE BEEN KILLED BUT JUST CHANGED TO ANOTHER STAR SYSTEM -> NO KILL
	//un->Kill();
}

// Broadcast a packet to a client's zone clients
void	ZoneMgr::broadcast( Client * clt, Packet * pckt )
{
    if( clt == NULL )
    {
        cout<<"Trying to send update without client" << endl;
        return;
    }
    if( clt->zone < 0 || clt->zone > zone_list.size() )
    {
        cout<<"Trying to send update to nonexistant zone " << clt->zone << endl;
        return;
    }

    // cout<<"Sending update to "<<(zone_list[clt->zone].size()-1)<<" clients"<<endl;
	for( LI i=zone_list[clt->zone].begin(); i!=zone_list[clt->zone].end(); i++)
	{
		// Broadcast to other clients
		if( clt->serial!= (*i)->serial)
		{
			cout<<"Sending update to client n� "<<(*i)->serial;
			cout<<endl;
			pckt->setNetwork( &(*i)->cltadr, (*i)->sock);
			pckt->bc_send( );
		}
	}
}

// Broadcast all snapshots
void	ZoneMgr::broadcastSnapshots( )
{
	ClientState cstmp;
	char buffer[MAXBUFFER];
	int i=0, j=0, p=0;
	LI k, l;
	NetBuffer netbuf;

	//cout<<"Sending snapshot for ";
	//int h_length = Packet::getHeaderLength();
	// Loop for all systems/zones
	for( i=0; i<zone_list.size(); i++)
	{
		// Check if system is non-empty
		if( zone_clients[i]>0)
		{
			//buffer = new char[zone_clients[i]*sizeof( ClientState)+h_length];

			/************* First method : build a snapshot buffer ***************/
			// It just look if positions or orientations have changed
			/*
			for( j=0, k=zone_list[i].begin(); k!=zone_list[i].end(); k++, j++)
			{
				// Check if position has changed
				memcpy( buffer+h_length+(j*sizeof( ClientState)), &(*k)->current_state, sizeof( ClientState));
			}
			// Then send all clients the snapshot
			Packet pckt;
			pckt.create( CMD_SNAPSHOT, zone_clients[i], buffer, zone_clients[j]*sizeof(ClientState), 0);
			for( j=0, k=zone_list[i].begin(); k!=zone_list[i].end(); k++, j++)
			{
				net->sendbuf( (*k)->sock, (char *) &pckt, pckt.getLength(), &(*k)->cltadr);
			}
			*/
			/************* Second method : send independently to each client a buffer of its zone  ***************/
			// It allows to check (for a given client) if other clients are far away (so we can only
			// send position, not orientation and stuff) and if other clients are visible to the given
			// client.
			// -->> Reduce bandwidth usage but increase CPU usage
			int	offset = 0, nbclients = 0;
			ObjSerial sertmp;
			Packet pckt;

			// Loop for all the zone's clients
			for( k=zone_list[i].begin(); k!=zone_list[i].end(); k++)
			{
				// If we don't want to send a client its own info set nbclients to zone_clients-1 for memory saving (ok little)
				nbclients = zone_clients[i]-1;
				netbuf.Reset();
				for( j=0, p=0, l=zone_list[i].begin(); l!=zone_list[i].end(); l++)
				{
					// Check if we are on the same client and that the client has moved !
					if( l!=k && !((*l)->current_state.getPosition()==(*l)->old_state.getPosition() && (*l)->current_state.getOrientation()==(*l)->old_state.getOrientation()))
					{
						// Client pointed by 'k' can see client pointed by 'l'
						// For now only check if the 'l' client is in front of the ship and not behind
						if( 1 /*(distance = this->isVisible( source_orient, source_pos, target_pos)) > 0*/)
						{
							// Test if client 'l' is far away from client 'k' = test radius/distance<=X
							// So we can send only position
							// Here distance should never be 0
							//ratio = radius/distance;
							if( 1 /* ratio > XX client not too far */)
							{
								// Mark as position+orientation+velocity update
								netbuf.addChar( CMD_FULLUPDATE);
								// Put the current client state in
								netbuf.addClientState( (*l)->current_state);
								// Increment the number of clients we send full info about
								j++;
							}
							// Here find a condition for which sending only position would be enough
							else if( 1 /* ratio>=1 far but still visible */)
							{
								// Mark as position update only
								netbuf.addChar( CMD_POSUPDATE);
								// Add the client serial
								netbuf.addShort( (*l)->serial);
								netbuf.addVector( (*l)->current_state.getPosition());
								// Increment the number of clients we send limited info about
								p++;
							}
						}
					}
					// Else : always send back to clients their own info or just ignore ?
					// Ignore for now
				}
				// Send snapshot to client k
				if( j+p > 0)
				{
					//cout<<"\tsend update for "<<(p+j)<<" clients"<<endl;
					pckt.send( CMD_SNAPSHOT, nbclients, netbuf.getData(), netbuf.getDataLength(), SENDANDFORGET, &((*k)->cltadr), (*k)->sock, __FILE__,	__LINE__);
				}
			}
		}
	}
}

// Send one by one a CMD_ADDLCIENT to the client for every ship in the star system we enter
void	ZoneMgr::sendZoneClients( Client * clt)
{
	LI k;
	int nbclients=0;
	Packet packet2;
	string savestr, xmlstr;
	NetBuffer netbuf;

	// Loop through client in the same zone to send their current_state and save and xml to "clt"
	for( k=zone_list[clt->zone].begin(); k!=zone_list[clt->zone].end(); k++)
	{
		// Test if *k is the same as clt in which case we don't need to send info
		if( clt!=(*k))
		{
			SaveNetUtil::GetSaveStrings( (*k), savestr, xmlstr);
			unsigned int savelen = savestr.length();
			unsigned int xmllen = xmlstr.length();
			// Add the ClientState at the beginning of the buffer
			netbuf.addClientState( (*k)->current_state);
			// Add the save and xml strings
			netbuf.addString( savestr);
			netbuf.addString( xmlstr);
			packet2.send( CMD_ENTERCLIENT, clt->serial, netbuf.getData(), netbuf.getDataLength(), SENDRELIABLE, &clt->cltadr, clt->sock, __FILE__, __LINE__);
			nbclients++;
		}
	}
	cout<<"\t>>> SENT INFO ABOUT "<<nbclients<<" OTHER SHIPS TO CLIENT SERIAL "<<clt->serial<<endl;
}

// Fills buffer with descriptions of clients in the same zone as our client
// Called after the client has been added in the zone so that it can get his
// own information/save from the server
int		ZoneMgr::getZoneClients( Client * clt, char * bufzone)
{
	LI k;
	int state_size;
	unsigned short nbt, nb;
	state_size = sizeof( ClientState);
	nbt = zone_clients[clt->zone];
	NetBuffer netbuf;

	cout<<"ZONE "<<clt->zone<<" - "<<nbt<<" clients"<<endl;
	netbuf.addShort( nbt);
	for( k=zone_list[clt->zone].begin(); k!=zone_list[clt->zone].end(); k++)
	{
		cout<<"SENDING : ";
		netbuf.addClientState( (*k)->current_state);
		(*k)->current_state.display();
	}

	return state_size*nbt;
}

double	ZoneMgr::isVisible( Quaternion orient, QVector src_pos, QVector tar_pos)
{
	double	dotp = 0;
	Matrix m;

	orient.to_matrix(m);
	QVector src_tar( m.getR());

	src_tar = tar_pos - src_pos;
	dotp = DotProduct( src_tar, (QVector) orient.v);

	return dotp;
}

/*** This is a copy of GFXSphereInFrustum from gl_matrix_hack.cpp avoiding
 * linking with a LOT of unecessary stuff
 */

/*
float	ZoneMgr::sphereInFrustum( const Vector &Cnt, float radius)
{
	float frust [6][4];
   int p;
   float d;
   for( p = 0; p < 5; p++ )//does not evaluate for yon
   {
      d = f[p][0] * Cnt.i + f[p][1] * Cnt.j + f[p][2] * Cnt.k + f[p][3];
      if( d <= -radius )
         return 0;
   }
   return d;
}
*/
