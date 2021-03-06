#include "planet_generic.h"
#include "gfx/mesh.h"
#include "galaxy_xml.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "universe_util.h"
#include "lin_time.h"
#include "planetary_orbit.h"
#include "vsfilesystem.h"
#include "unit.h"
#include "planet.h"
#include "universe.h"

using std::endl;

char * getnoslash( char *inp )
{
    char *tmp = inp;
    for (unsigned int i = 0; inp[i] != '\0'; i++)
        if (inp[i] == '/' || inp[i] == '\\')
            tmp = inp+i+1;
    return tmp;
}

string getCargoUnitName( const char *textname )
{
    char *tmp2 = strdup( textname );
    char *tmp  = getnoslash( tmp2 );
    unsigned int i;
    for (i = 0; tmp[i] != '\0' && (isalpha( tmp[i] ) || tmp[i] == '_'); i++) {}
    if (tmp[i] != '\0')
        tmp[i] = '\0';
    string retval( tmp );
    free( tmp2 );
    return retval;
}

string GetElMeshName( string name, string faction, char direction )
{
    using namespace VSFileSystem;
    char    strdir[2]     = {direction, 0};
    string  elxmesh       = string( strdir )+"_elevator.bfxm";
    string  elevator_mesh = name+"_"+faction+elxmesh;
    VSFile  f;
    VSError err = f.OpenReadOnly( elevator_mesh, MeshFile );
    if (err > Ok)
        f.Close();
    else elevator_mesh = name+elxmesh;
    return elevator_mesh;
}

Vector Planet::AddSpaceElevator( const std::string &name, const std::string &faction, char direction )
{
    Vector dir, scale;
    switch (direction)
    {
    case 'u':
        dir.Set( 0, 1, 0 );
        break;
    case 'd':
        dir.Set( 0, -1, 0 );
        break;
    case 'l':
        dir.Set( -1, 0, 0 );
        break;
    case 'r':
        dir.Set( 1, 0, 0 );
        break;
    case 'b':
        dir.Set( 0, 0, -1 );
        break;
    default:
        dir.Set( 0, 0, 1 );
        break;
    }
    Matrix ElevatorLoc( Vector( dir.j, dir.k, dir.i ), dir, Vector( dir.k, dir.i, dir.j ) );
    scale = dir*radius+Vector( 1, 1, 1 )-dir;
    Mesh  *shield = meshdata.back();
    string elevator_mesh = GetElMeshName( name, faction, direction );     //filename
    Mesh  *tmp    = meshdata.back() = Mesh::LoadMesh( elevator_mesh.c_str(),
                                                      scale,
                                                      FactionUtil::
                                                      GetFactionIndex( faction ),
                                                      NULL );

    meshdata.push_back( shield );
    {
        //subunit computations
        Vector mn( tmp->corner_min() );
        Vector mx( tmp->corner_max() );
        if (dir.Dot( Vector( 1, 1, 1 ) ) > 0)
            ElevatorLoc.p.Set( dir.i*mx.i, dir.j*mx.j, dir.k*mx.k );
        else
            ElevatorLoc.p.Set( -dir.i*mn.i, -dir.j*mn.j, -dir.k*mn.k );
        Unit *un = new GameUnit< Unit >( name.c_str(), true, FactionUtil::GetFactionIndex( faction ), "", NULL );
        if (pImage->dockingports.back().GetPosition().MagnitudeSquared() < 10)
            pImage->dockingports.clear();
        pImage->dockingports.push_back( DockingPorts( ElevatorLoc.p, un->rSize()*1.5, 0, DockingPorts::Type::INSIDE ) );
        un->SetRecursiveOwner( this );
        un->SetOrientation( ElevatorLoc.getQ(), ElevatorLoc.getR() );
        un->SetPosition( ElevatorLoc.p );
        SubUnits.prepend( un );
    }
    return dir;
}

void Planet::endElement() {}

Planet* Planet::GetTopPlanet( int level )
{
    if (level > 2) {
        un_iter satiterator = satellites.createIterator();
        assert( *satiterator );
        if ( (*satiterator)->isUnit() == PLANETPTR ) {
            return ( (Planet*) (*satiterator) )->GetTopPlanet( level-1 );
        } else {
            VSFileSystem::vs_fprintf( stderr, "Planets are unable to orbit around units" );
            return NULL;
        }
    } else {
        return this;
    }
}

void Planet::AddSatellite( Unit *orbiter )
{
    satellites.prepend( orbiter );
    orbiter->SetOwner( this );
}

extern float ScaleJumpRadius( float );
extern Flightgroup * getStaticBaseFlightgroup( int faction );

Unit* Planet::beginElement( QVector x,
                            QVector y,
                            float vely,
                            const Vector &rotvel,
                            float pos,
                            float gravity,
                            float radius,
                            const string &filename,
                            const string &technique,
                            const string &unitname,
                            BLENDFUNC blendSrc,
                            BLENDFUNC blendDst,
                            const vector< string > &dest,
                            int level,
                            const GFXMaterial &ourmat,
                            const vector< GFXLightLocal > &ligh,
                            bool isunit,
                            int faction,
                            string fullname,
                            bool inside_out )
{
    //this function is OBSOLETE
    Unit *un = NULL;
    if (level > 2) {
        un_iter satiterator = satellites.createIterator();
        assert( *satiterator );
        if ( (*satiterator)->isUnit() == PLANETPTR ) {
            un = ( (Planet*) (*satiterator) )->beginElement( x, y, vely, rotvel, pos,
                                                             gravity, radius,
                                                             filename, technique, unitname,
                                                             blendSrc, blendDst,
                                                             dest,
                                                             level-1,
                                                             ourmat, ligh,
                                                             isunit,
                                                             faction, fullname,
                                                             inside_out );
        } else {
            VSFileSystem::vs_fprintf( stderr, "Planets are unable to orbit around units" );
        }
    } else {
        if (isunit == true) {
            Unit *sat_unit  = NULL;
            Flightgroup *fg = getStaticBaseFlightgroup( faction );
            satellites.prepend( sat_unit = new GameUnit< Unit >( filename.c_str(), false, faction, "", fg, fg->nr_ships-1 ) );
            sat_unit->setFullname( fullname );
            un = sat_unit;
            un_iter satiterator( satellites.createIterator() );
            (*satiterator)->SetAI( new PlanetaryOrbit( *satiterator, vely, pos, x, y, QVector( 0, 0, 0 ), this ) );
            (*satiterator)->SetOwner( this );
        } else {
            // For debug
//            BOOST_LOG_TRIVIAL(trace) << "name" << " : " << filename << " : " << unitname << endl;
//            BOOST_LOG_TRIVIAL(trace) << "R/X: " << x.i << " : " << x.j << " : " << x.k << endl;
//            BOOST_LOG_TRIVIAL(trace) << "S/Y: " << y.i << " : " << y.j << " : " << y.k << endl;
//            BOOST_LOG_TRIVIAL(trace) << "CmpRotVel: " << rotvel.i << " : " <<
//                    rotvel.j << " : " << rotvel.k << endl;
//            BOOST_LOG_TRIVIAL(trace) << vely << " : " << pos << " : " << gravity << " : " << radius << endl;
//            BOOST_LOG_TRIVIAL(trace) << dest.size() << " : " << "orbit_center" << " : " << ligh.size() << endl;
//            BOOST_LOG_TRIVIAL(trace) << blendSrc << " : " << blendDst << " : " << inside_out << endl;




            Planet *p;
            if (dest.size() != 0)
                radius = ScaleJumpRadius( radius );
            satellites.prepend( p = new GamePlanet( x, y, vely, rotvel, pos, gravity, radius,
                                                               filename, technique, unitname,
                                                               blendSrc, blendDst, dest,
                                                               QVector( 0, 0, 0 ), this, ourmat, ligh, faction, fullname, inside_out ) );
            un = p;
            p->SetOwner( this );
            BOOST_LOG_TRIVIAL(trace) << "Created planet " << fullname << " of type " << p->fullname << " orbiting " << this->fullname << endl;

        }
    }
    return un;
}

Planet::Planet() :
    Unit( 0 )
    , radius( 0.0f )
    , satellites()
{
    inside = false;
    //Not needed as Unit default constructor is called and already does Init
    //Init();
    terraintrans = NULL;
    atmospheric  = false;
    //Force shields to 0
//    memset( &(this->shield), 0, sizeof (Unit::shield) ); //Let's not use memset on non-trivial stuff
    this->shield.number=2;
    this->shield.recharge=0;
    this->shield.shield2fb.frontmax=0;
    this->shield.shield2fb.backmax=0;
    this->shield.shield2fb.front=0;
    this->shield.shield2fb.back=0;
}

void Planet::InitPlanet( QVector x,
                         QVector y,
                         float vely,
                         const Vector &rotvel,
                         float pos,
                         float gravity,
                         float radius,
                         const string &filename,
                         const string &technique,
                         const string &unitname,
                         const vector< string > &dest,
                         const QVector &orbitcent,
                         Unit *parent,
                         int faction,
                         string fullname,
                         bool inside_out,
                         unsigned int lights_num )
{
    atmosphere = NULL;
    terrain    = NULL;
    static float bodyradius = XMLSupport::parse_float( vs_config->getVariable( "graphics", "star_body_radius", ".33" ) );
    if (lights_num)
        radius *= bodyradius;
    inside = false;
    curr_physical_state.position = prev_physical_state.position = cumulative_transformation.position = orbitcent+x;
    Init();
    //static int neutralfaction=FactionUtil::GetFaction("neutral");
    //this->faction = neutralfaction;
    killed = false;
    bool notJumppoint = dest.empty();
    for (unsigned int i = 0; i < dest.size(); ++i)
        AddDestination( dest[i] );
    //name = "Planet - ";
    //name += textname;
    name = fullname;
    this->fullname = name;
    this->radius   = radius;
    this->gravity  = gravity;
    static float densityOfRock = XMLSupport::parse_float( vs_config->getVariable( "physics", "density_of_rock", "3" ) );
    static float densityOfJumpPoint =
        XMLSupport::parse_float( vs_config->getVariable( "physics", "density_of_jump_point", "100000" ) );
    //static  float massofplanet = XMLSupport::parse_float(vs_config->getVariable("physics","mass_of_planet","10000000"));
    hull = (4./3)*M_PI*radius*radius*radius*(notJumppoint ? densityOfRock : densityOfJumpPoint);
    this->Mass   = (4./3)*M_PI*radius*radius*radius*( notJumppoint ? densityOfRock : (densityOfJumpPoint/100000) );
    SetAI( new PlanetaryOrbit( this, vely, pos, x, y, orbitcent, parent ) );     //behavior
    terraintrans = NULL;

    colTrees     = NULL;
    SetAngularVelocity( rotvel );
    // The docking port is 20% bigger than the planet
    static float planetdockportsize    = XMLSupport::parse_float( vs_config->getVariable( "physics", "planet_port_size", "1.2" ) );
    static float planetdockportminsize =
        XMLSupport::parse_float( vs_config->getVariable( "physics", "planet_port_min_size", "300" ) );
    if ( (!atmospheric) && notJumppoint ) {
        float dock = radius*planetdockportsize;
        if (dock-radius < planetdockportminsize)
            dock = radius+planetdockportminsize;
        pImage->dockingports.push_back( DockingPorts( Vector( 0, 0, 0 ), dock, 0, DockingPorts::Type::CONNECTED_OUTSIDE ) );
    }
    string tempname = unitname.empty() ? ::getCargoUnitName( filename.c_str() ) : unitname;
    setFullname( tempname );

    int    tmpfac   = faction;
    if (UniverseUtil::LookupUnitStat( tempname, FactionUtil::GetFactionName( faction ), "Cargo_Import" ).length() == 0)
        tmpfac = FactionUtil::GetPlanetFaction();
    Unit  *un = new GameUnit< Unit >( tempname.c_str(), true, tmpfac );

    static bool smartplanets = XMLSupport::parse_bool( vs_config->getVariable( "physics", "planets_can_have_subunits", "false" ) );
    if ( un->name != string( "LOAD_FAILED" ) ) {
        pImage->cargo = un->GetImageInformation().cargo;
        pImage->CargoVolume   = un->GetImageInformation().CargoVolume;
        pImage->UpgradeVolume = un->GetImageInformation().UpgradeVolume;
        VSSprite *tmp = pImage->pHudImage;
        pImage->pHudImage     = un->GetImageInformation().pHudImage;
        un->GetImageInformation().pHudImage = tmp;
        maxwarpenergy = un->WarpCapData();
        if (smartplanets) {
            SubUnits.prepend( un );
            un->SetRecursiveOwner( this );
            this->SetTurretAI();
            un->SetTurretAI();              //allows adding planetary defenses, also allows launching fighters from planets, interestingly
            un->name = "Defense_grid";
        }
        static bool neutralplanets =
            XMLSupport::parse_bool( vs_config->getVariable( "physics", "planets_always_neutral", "true" ) );
        if (neutralplanets) {
            static int neutralfaction = FactionUtil::GetNeutralFaction();
            this->faction = neutralfaction;
        } else {
            this->faction = faction;
        }
    }
    if ( un->name == string( "LOAD_FAILED" ) || (!smartplanets) )
        un->Kill();
}

Planet::Planet( QVector x,
                QVector y,
                float vely,
                const Vector &rotvel,
                float pos,
                float gravity,
                float radius,
                const string &filename,
                const string &technique,
                const string &unitname,
                const vector< string > &dest,
                const QVector &orbitcent,
                Unit *parent,
                int faction,
                string fullname,
                bool inside_out,
                unsigned int lights_num )
{
    inside = false;
    terraintrans = NULL;
    atmospheric  = false;
    this->InitPlanet( x, y, vely, rotvel,
                      pos,
                      gravity, radius,
                      filename, technique, unitname,
                      dest,
                      orbitcent, parent,
                      faction, fullname,
                      inside_out,
                      lights_num );
    corner_min.i = corner_min.j = corner_min.k = -this->radius;
    corner_max.i = corner_max.j = corner_max.k = this->radius;
    this->radial_size = this->radius;
    for (unsigned int i = 0; i < lights_num; i++) {
        int l = -1;
        lights.push_back( l );
    }
    //Force shields to 0
    this->shield.number=2;
    this->shield.recharge=0;
    this->shield.shield2fb.frontmax=0;
    this->shield.shield2fb.backmax=0;
    this->shield.shield2fb.front=0;
    this->shield.shield2fb.back=0;
    if ( meshdata.empty() ) meshdata.push_back( NULL );
}

string Planet::getHumanReadablePlanetType() const
{
    //static std::map<std::string, std::string> planetTypes (readPlanetTypes("planet_types.xml"));
    //return planetTypes[getCargoUnitName()];
    return _Universe->getGalaxy()->getPlanetNameFromTexture( getCargoUnitName() );
}

Planet::~Planet()
{
    if (terraintrans) {
        Matrix *tmp = new Matrix();
        *tmp = cumulative_transformation_matrix;
        //terraintrans->SetTransformation (tmp);
    }
}

void Planet::Kill( bool erasefromsave )
{
    un_iter iter;
    Unit   *tmp;
    for (iter = satellites.createIterator();
         (tmp = *iter);
         ++iter)
        tmp->SetAI( new Order );
    /* probably not FIXME...right now doesn't work on paged out systems... not a big deal */
    satellites.clear();
    insiders.clear();
    Unit::Kill( erasefromsave );
}

bool operator==(const Planet& lhs, const Planet& rhs)
{
    bool equal = true;
    if(lhs.inside != rhs.inside) {
        equal = false;
        BOOST_LOG_TRIVIAL(trace) << "inside: " << lhs.inside << " != " << rhs.inside << endl;
    }

    if(lhs.atmospheric != rhs.atmospheric) {
        equal = false;
        BOOST_LOG_TRIVIAL(trace) << "atmospheric: " << lhs.atmospheric << " != " << rhs.atmospheric << endl;
    }

    // TODO: turn floating point comparisons into a function
    if(std::fabs(lhs.radius - rhs.radius) > 0.001f) {
        equal = false;
        BOOST_LOG_TRIVIAL(trace) << "radius: " << lhs.radius << " != " << rhs.radius << endl;
    }

    if(std::fabs(lhs.gravity - rhs.gravity) > 0.001f) {
        equal = false;
        BOOST_LOG_TRIVIAL(trace) << "gravity: " << lhs.gravity << " != " << rhs.gravity << endl;
    }

    return equal;
}
