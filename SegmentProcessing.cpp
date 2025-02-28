#include "common.h"
#include "SegmentProcessing.h"
#include "OcclusionTest.h"
#include "ContentLoader.h"
#include "GameBuildings.h"
#include "SpriteMaps.h"
#include "GameConfiguration.h"
#include "StonesenseState.h"

#include <array>

//big look up table
uint8_t rampblut[] =
    // generated by blutmaker.py
{
    1 ,  2 ,  8 ,  2 ,  4 , 12 ,  4 , 12 ,  9 ,  2 , 21 ,  2 ,  4 , 12 ,  4 , 12 ,
    5 , 16 ,  5 , 16 , 13 , 13 , 13 , 12 ,  5 , 16 ,  5 , 16 , 13 , 13 , 13 , 16 ,
    7 ,  2 , 14 ,  2 ,  4 , 12 ,  4 , 12 , 20 , 26 , 25 , 26 ,  4 , 12 ,  4 , 12 ,
    5 , 16 ,  5 , 16 , 13 , 16 , 13 , 16 ,  5 , 16 ,  5 , 16 , 13 , 16 , 13 , 16 ,
    3 , 10 ,  3 , 10 , 17 , 12 , 17 , 12 ,  3 , 10 , 26 , 10 , 17 , 17 , 17 , 17 ,
    11 , 10 , 11 , 16 , 11 , 26 , 17 , 12 , 11 , 16 , 11 , 16 , 13 , 13 , 17 , 16 ,
    3 , 10 ,  3 , 10 , 17 , 17 , 17 , 17 ,  3 , 10 , 26 , 10 , 17 , 17 , 17 , 17 ,
    11 , 11 , 11 , 16 , 11 , 11 , 17 , 14 , 11 , 16 , 11 , 16 , 17 , 17 , 17 , 13 ,
    6 ,  2 , 19 ,  2 ,  4 , 12 ,  4 , 12 , 15 ,  2 , 24 ,  2 ,  4 , 12 ,  4 , 12 ,
    5 , 16 , 26 , 16 , 13 , 16 , 13 , 16 ,  5 , 16 , 26 , 16 , 13 , 16 , 13 , 16 ,
    18 ,  2 , 22 ,  2 , 26 , 12 , 26 , 12 , 23 , 26 , 26 , 26 , 26 , 12 , 26 , 12 ,
    5 , 16 , 26 , 16 , 13 , 16 , 13 , 16 ,  5 , 16 , 26 , 16 , 13 , 16 , 13 , 16 ,
    3 , 10 ,  3 , 10 , 17 , 10 , 17 , 17 ,  3 , 10 , 26 , 10 , 17 , 17 , 17 , 17 ,
    11 , 10 , 11 , 16 , 17 , 10 , 17 , 17 , 11 , 16 , 11 , 16 , 17 , 15 , 17 , 12 ,
    3 , 10 ,  3 , 10 , 17 , 17 , 17 , 17 ,  3 , 10 , 26 , 10 , 17 , 17 , 17 , 17 ,
    11 , 16 , 11 , 16 , 17 , 16 , 17 , 10 , 11 , 16 , 11 , 16 , 17 , 11 , 17 , 26
};

inline bool isTileHighRampEnd(uint32_t x, uint32_t y, uint32_t z, WorldSegment* segment, dirRelative dir)
{
    Tile* tile = segment->getTileRelativeTo( x, y, z, dir);
    if(!tile) {
        return false;
    }
    if(tile->tileShapeBasic()!=df::enums::tiletype_shape_basic::Wall) {
        return false;
    }
    return IDisWall( tile->tileType );
}

inline int tileWaterDepth(uint32_t x, uint32_t y, uint32_t z, WorldSegment* segment, dirRelative dir)
{
    Tile* tile = segment->getTileRelativeTo( x, y, z, dir);

    if(!tile ||
        tile->designation.bits.liquid_type != df::tile_liquid::Water) {
        return 0;
    }
    return tile->designation.bits.flow_size;
}

inline bool isTileHighRampTop(uint32_t x, uint32_t y, uint32_t z, WorldSegment* segment, dirRelative dir)
{
    using df::tiletype_shape_basic;
    Tile* tile = segment->getTileRelativeTo( x, y, z, dir);
    if(!tile) {
        return false;
    }
    if(tile->tileShapeBasic()!=tiletype_shape_basic::Floor
        && tile->tileShapeBasic()!=tiletype_shape_basic::Ramp
        && tile->tileShapeBasic()!=tiletype_shape_basic::Stair) {
        return false;
    }
    if(tile->tileShapeBasic()!=tiletype_shape_basic::Wall) {
        return true;
    }
    return !IDisWall( tile->tileType );
}

uint8_t CalculateRampType(uint32_t x, uint32_t y, uint32_t z, WorldSegment* segment)
{
    const std::array<dirRelative, 8> dirs = {
        eUp,
        eUpRight,
        eRight,
        eDownRight,
        eDown,
        eDownLeft,
        eLeft,
        eUpLeft
    };

    int32_t ramplookup = 0;
    for (unsigned int i = 0; i < dirs.size(); i++) {
        if (isTileHighRampEnd(x, y, z, segment, dirs[i]) &&
            isTileHighRampTop(x, y, z + 1, segment, dirs[i])) {
            ramplookup ^= 1 << i;
        }
    }

    // creation should ensure in range
    if (ramplookup > 0) {
        return rampblut[ramplookup];
    }

    for (unsigned int i = 0; i < dirs.size(); i++) {
        if (isTileHighRampEnd(x, y, z, segment, dirs[i])) {
            ramplookup ^= 1 << i;
        }
    }

    // creation should ensure in range
    return rampblut[ramplookup];
}

bool checkFloorBorderRequirement(WorldSegment* segment, int x, int y, int z, dirRelative offset)
{
    using df::tiletype_shape_basic;
    Tile* bHigh = segment->getTileRelativeTo(x, y, z, offset);
    if (bHigh &&
        (bHigh->tileShapeBasic()==tiletype_shape_basic::Floor
         || bHigh->tileShapeBasic()==tiletype_shape_basic::Ramp
         || bHigh->tileShapeBasic()==tiletype_shape_basic::Wall)) {
        return false;
    }
    Tile* bLow = segment->getTileRelativeTo(x, y, z-1, offset);
    return !bLow || bLow->tileShapeBasic()!=tiletype_shape_basic::Ramp;
}

bool isTileOnVisibleEdgeOfSegment(WorldSegment* segment, Tile* b)
{
    int32_t x = b->x;
    int32_t y = b->y;
    int32_t z = b->z;
    segment->ConvertToSegmentLocal(x, y, z);

    if (int(b->z) == segment->segState.Position.z + segment->segState.Size.z - 2) {
        return true;
    }
    if (x == segment->segState.Size.x - 2 ||
        y == segment->segState.Size.y - 2) {
        return true;
    }

    return false;
}

bool areNeighborsVisible(WorldSegment* segment, Tile* b)
{
    const std::array<dirRelative, 5> dirs = {eUp, eDown, eLeft, eRight, eAbove};

    Tile *temp;
    for (const dirRelative &dir : dirs) {
        temp = segment->getTileRelativeTo(b->x, b->y, b->z, dir);
        if(!temp || !(temp->designation.bits.hidden)) {
            return true;
        }
    }
    return false;
}

/**
* returns true iff the tile is enclosed by other solid tiles, and is itself solid
*/
bool enclosed(WorldSegment* segment, Tile* b)
{
    if(!IDisWall(b->tileType)) {
        return false;
    }

    Tile* temp;
    temp = segment->getTile(b->x, b->y, b->z+1);
    if(!temp || !IDhasOpaqueFloor(temp->tileType)) {
        return false;
    }

    const std::array<dirRelative, 5> dirs = {eUp, eDown, eLeft, eRight};
    for (const dirRelative &dir : dirs) {
        temp = segment->getTileRelativeTo(b->x, b->y, b->z, dir);
        if (!temp || !IDisWall(temp->tileType)) {
            return false;
        }
    }

    return true;
}

/**
* checks to see if the tile is a potentially viewable hidden tile
*  if so, put the black mask tile overtop
*  if not, makes tile not visible
*/
inline void maskTile(WorldSegment * segment, Tile* b)
{
    //include hidden tiles as shaded black, make remaining invisible
    if( b->designation.bits.hidden ) {
        if( isTileOnVisibleEdgeOfSegment(segment, b)
            || areNeighborsVisible(segment, b) ) {
            b->building.type = BUILDINGTYPE_BLACKBOX;
        } else {
            b->visible = false;
        }
    }
}

/**
* checks to see if the tile is a potentially viewable hidden tile
*  if not, makes tile not visible
* ASSUMES YOU ARE NOT ON THE SEGMENT EDGE
*/
inline void enclosedTile(WorldSegment * segment, Tile* b)
{
    //make tiles that are impossible to see invisible
    if( b->designation.bits.hidden
        && (enclosed(segment, b)) ) {
        b->visible = false;
    }
}

/**
* enables visibility and disables fog for the first layer of water
*  below visible space
*/
inline void unhideWaterFromAbove(WorldSegment * segment, Tile * b)
{
    auto& contentLoader = stonesenseState.contentLoader;

    if ( b->designation.bits.flow_size == 0
        || isTileOnTopOfSegment(segment, b)
        || !(b->designation.bits.hidden || b->fog_of_war) ) {
        return;
    }

    Tile * above = segment->getTile(b->x, b->y, b->z+1);
    if (above && (IDhasOpaqueFloor(above->tileType) || above->designation.bits.flow_size != 0)) {
        return;
    }

    bool isAdventure = contentLoader->gameMode.g_mode == GAMEMODE_ADVENTURE;
    if(!above || !above->designation.bits.hidden || !(isAdventure && above->fog_of_war)) {
        b->designation.bits.hidden = false;
        if(isAdventure) {
            b->fog_of_war = false;
        }
        if (b->building.type == BUILDINGTYPE_BLACKBOX) {
            b->building.type = (df::building_type) BUILDINGTYPE_NA;
        }
    }
}

void determineDepthBorders(WorldSegment * segment, Tile* b)
{
    using df::tiletype_shape_basic;
    if( b->tileShapeBasic()==tiletype_shape_basic::Floor ) {
        b->depthBorderWest = checkFloorBorderRequirement(segment, b->x, b->y, b->z, eLeft);
        b->depthBorderNorth = checkFloorBorderRequirement(segment, b->x, b->y, b->z, eUp);

        Tile* belowTile = segment->getTileRelativeTo(b->x, b->y, b->z, eBelow);
        if(!belowTile || (belowTile->tileShapeBasic()!=tiletype_shape_basic::Wall && belowTile->tileShapeBasic()!=tiletype_shape_basic::Ramp)) {
            b->depthBorderDown = true;
        }
    } else if( b->tileShapeBasic()==tiletype_shape_basic::Wall && !wallShouldNotHaveBorders( b->tileType )) {
        Tile* leftTile = segment->getTileRelativeTo(b->x, b->y, b->z, eLeft);
        Tile* upTile = segment->getTileRelativeTo(b->x, b->y, b->z, eUp);
        if(!leftTile || (leftTile->tileShapeBasic()!=tiletype_shape_basic::Wall && leftTile->tileShapeBasic()!=tiletype_shape_basic::Ramp)) {
            b->depthBorderWest = true;
        }
        if(!upTile || (upTile->tileShapeBasic()!=tiletype_shape_basic::Wall && upTile->tileShapeBasic()!=tiletype_shape_basic::Ramp)) {
            b->depthBorderNorth = true;
        }
        Tile* belowTile = segment->getTileRelativeTo(b->x, b->y, b->z, eBelow);
        if(!belowTile || (belowTile->tileShapeBasic()!=tiletype_shape_basic::Wall && belowTile->tileShapeBasic()!=tiletype_shape_basic::Ramp)) {
            b->depthBorderDown = true;
        }
    }
}

/**
 * sets up the tile border conditions of the given block
 */
void arrangeTileBorders(WorldSegment * segment, Tile* b)
{
    //add edges to tiles and floors
    std::array<dirRelative, 8> directions = { eUpLeft, eUp, eUpRight, eRight, eDownRight, eDown, eDownLeft, eLeft };
    std::array<Tile*, 8> dirs = {};

    for (unsigned int i = 0; i < directions.size(); i++) {
        dirs[i] = segment->getTileRelativeTo(b->x, b->y, b->z, directions[i]);
    }

    b->obscuringBuilding = false;
    b->obscuringCreature = false;

    if ((dirs[0] && dirs[0]->occ.bits.unit) || (dirs[1] && dirs[1]->occ.bits.unit) ||
        (dirs[7] && dirs[7]->occ.bits.unit)) {
        b->obscuringCreature = true;
    }

    using df::tiletype_shape_basic;
    auto isObscurableBuilding = [&](Tile *tile) {
        return tile && tile->building.type != BUILDINGTYPE_NA &&
               tile->building.type != BUILDINGTYPE_BLACKBOX &&
               tile->building.type != df::enums::building_type::Civzone &&
               tile->building.type != df::enums::building_type::Stockpile;
    };
    if (isObscurableBuilding(dirs[0]) || isObscurableBuilding(dirs[1]) ||
        isObscurableBuilding(dirs[7])) {
        b->obscuringBuilding = true;
    }

    determineDepthBorders(segment, b);

    b->wallborders = 0;
    b->rampborders = 0;
    b->upstairborders = 0;
    b->downstairborders = 0;
    b->floorborders = 0;
    b->lightborders = 0;

    // Determine which borders should be present
    for (unsigned int i = 0; i < dirs.size(); i++) {
        if (!dirs[i]) {
            continue;
        }

        switch (dirs[i]->tileShapeBasic()) {
        case tiletype_shape_basic::Wall:
            b->wallborders |= 1 << i;
            break;
        case tiletype_shape_basic::Ramp:
            b->rampborders |= 1 << i;
            break;
        case tiletype_shape_basic::Floor:
            b->floorborders |= 1 << i;
            break;
        default:
            break;
        }

        using df::tiletype_shape;
        switch (dirs[i]->tileShape()) {
        case tiletype_shape::STAIR_UP:
            b->upstairborders |= 1 << i;
            break;
        case tiletype_shape::STAIR_DOWN:
            b->downstairborders |= 1 << i;
            break;
        case tiletype_shape::STAIR_UPDOWN:
            b->upstairborders |= 1 << i;
            b->downstairborders |= 1 << i;
            break;
        default:
            break;
        }

        if (!dirs[i]->designation.bits.outside) {
            b->lightborders |= 1 << i;
        }
    }
    b->lightborders = ~b->lightborders;
    b->openborders = ~(b->floorborders|b->rampborders|b->wallborders|b->downstairborders|b->upstairborders);
}

/**
 * add extra sprites for buildings, trees, etc
 */
void addSegmentExtras(WorldSegment * segment)
{

    uint32_t numtiles = segment->getNumTiles();

    for(uint32_t i=0; i < numtiles; i++) {
        Tile* b = segment->getTile(i);

        if(!b || !b->visible) {
            continue;
        }

        if(!stonesenseState.ssConfig.show_hidden_tiles && b->designation.bits.hidden) {
            continue;
        }

        //Grass
        using df::tiletype_material;
        if(b->grasslevel > 0 && (
             (b->tileMaterial() == tiletype_material::GRASS_LIGHT) ||
             (b->tileMaterial() == tiletype_material::GRASS_DARK) ||
             (b->tileMaterial() == tiletype_material::GRASS_DEAD) ||
             (b->tileMaterial() == tiletype_material::GRASS_DRY))) {
                c_tile_tree * vegetationsprite = nullptr;
                vegetationsprite = getVegetationTree(stonesenseState.contentLoader->grassConfigs,b->grassmat,true,true);
            if(vegetationsprite) {
                vegetationsprite->insert_sprites(segment, b->x, b->y, b->z, b);
            }
        }

        //populate trees
        if(b->tree.index != 0) {
            c_tile_tree * Tree = GetTreeVegetation(b->tileShape(), b->tileSpecial(), b->tree.index );
            if (Tree)
                Tree->insert_sprites(segment, b->x, b->y, b->z, b);
        }

        //setup building sprites
        if( b->building.type != BUILDINGTYPE_NA && b->building.type != BUILDINGTYPE_BLACKBOX) {
            loadBuildingSprites( b);
        }

        //setup deep water
        if( b->designation.bits.flow_size == 7 && b->designation.bits.liquid_type == df::tile_liquid::Water) {
            int topdepth = tileWaterDepth(b->x, b->y, b->z, segment, eAbove);
            if(topdepth != 0) {
                b->deepwater = true;
            }
        }

        //setup ramps
        if(b->tileShapeBasic()==df::tiletype_shape_basic::Ramp) {
            b->rampindex = CalculateRampType(b->x, b->y, b->z, segment);
        }

        //setup tile borders
        arrangeTileBorders(segment, b);
    }
}

void optimizeSegment(WorldSegment * segment)
{
    //do misc beautification

    uint32_t numtiles = segment->getNumTiles();

    for(uint32_t i=0; i < numtiles; i++) {
        Tile* b = segment->getTile(i);

        if(!b) {
            continue;
        }
        auto& ssConfig = stonesenseState.ssConfig;

        //try to mask away tiles that are flagged hidden
        if(!ssConfig.show_hidden_tiles ) {
            //unhide any liquids that are visible from above
            unhideWaterFromAbove(segment, b);
            if( ssConfig.show_designations
                && containsDesignations(b->designation, b->occ) ) {
                b->visible = true;
            } else if( ssConfig.shade_hidden_tiles ) {
                maskTile(segment, b);
            } else if( b->designation.bits.hidden ) {
                b->visible = false;
            }
        }

        if(!b->visible) {
            continue;
        }

        if( !isTileOnVisibleEdgeOfSegment(segment, b)
            && (b->tileType!=df::tiletype::OpenSpace
            || b->designation.bits.flow_size != 0
            || (b->occ.bits.unit && b->creature)
            || b->building.type != BUILDINGTYPE_NA
            || b->tileeffect.type != (df::flow_type) INVALID_INDEX)) {

            //hide any tiles that are totally surrounded
            enclosedTile(segment, b);

            if(!b->visible) {
                continue;
            }

            //next see if the tile is behind something
            if(ssConfig.occlusion) {
                occlude_tile(b);
            }
        }
    }
}

void beautifySegment(WorldSegment * segment)
{
    if(!segment) {
        return;
    }

    clock_t starttime = clock();

    optimizeSegment(segment);
    addSegmentExtras(segment);

    segment->processed = true;
    stonesenseState.stoneSenseTimers.beautify_time.update(clock() - starttime);
}
