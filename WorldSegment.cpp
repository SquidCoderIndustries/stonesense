#include "common.h"
#include "WorldSegment.h"
#include "GUI.h"
#include "ContentLoader.h"
#include "Creatures.h"
#include "GameConfiguration.h"
#include "StonesenseState.h"

const GameState SegmentWrap::zeroState =
{
    df::coord::coord(),0,df::coord::coord(),df::coord::coord(),df::coord::coord(),df::coord::coord(),0
};

void WorldSegment::CorrectTileForSegmentOffset(auto& xin, auto& yin, auto& zin)
{
    xin -= segState.Position.x;
    yin -= segState.Position.y; //Position.y;
    zin -= segState.Position.z - 1; //need to remove the offset
}

void WorldSegment::CorrectTileForSegmentRotation(auto& x, auto& y, auto& z)
{
    auto oldx = x;
    auto oldy = y;

    if(segState.Rotation == 1) {
        x = segState.Size.x - oldy -1;
        y = oldx;
    }
    if(segState.Rotation == 2) {
        x = segState.Size.x - oldx -1;
        y = segState.Size.y - oldy -1;
    }
    if(segState.Rotation == 3) {
        x = oldy;
        y = segState.Size.y - oldx -1;
    }
}

//Converts a set of world coordinates into local coordinates, taking into account view rotation.
bool WorldSegment::ConvertToSegmentLocal(auto & x, auto & y, auto & z)
{
    auto lx = x;
    auto ly = y;
    auto lz = z;

    lx -= segState.Position.x;
    ly -= segState.Position.y;
    lz -= segState.Position.z;

    CorrectTileForSegmentRotation(lx, ly, lz);

    //even if we return false, we still might want to use the new coords, maybe.
    x = lx;
    y = ly;
    z = lz;

    if(lx < 0 || lx >= segState.Size.x) {
        return false;
    }
    if(ly < 0 || ly >= segState.Size.y) {
        return false;
    }
    if(lz < 0 || lz >= segState.Size.z) {
        return false;
    }

    return true;
}

uint32_t WorldSegment::ConvertLocalToIndex(auto x, auto y, auto z)
{
    return (uint32_t) (x + (y + (z*segState.Size.y))*segState.Size.x );
}

//Returns a blank tile at the specified world coordinates.
Tile* WorldSegment::ResetTile(auto x, auto y, auto z, df::tiletype type)
{
    auto lx = x;
    auto ly = y;
    auto lz = z;

    if(!ConvertToSegmentLocal(lx, ly, lz)){
        return NULL;
    }

    uint32_t index = ConvertLocalToIndex(lx, ly, lz);

    Tile* tptr = &(tiles[index]);

    tptr->Attach(this, type, x, y, z);
    return tptr;
}

//Returns an existing tile at the specified world coordinates.
Tile* WorldSegment::getTile(auto x, auto y, auto z)
{
    auto lx = x;
    auto ly = y;
    auto lz = z;

    if(!ConvertToSegmentLocal(lx, ly, lz)){
        return NULL;
    }

    uint32_t index = ConvertLocalToIndex(lx, ly, lz);

    return getTile(index);
}

Tile* WorldSegment::getTileRelativeTo(auto x, auto y, auto z,  dirRelative direction)
{
    auto lx = x;
    auto ly = y;
    auto lz = z;

    if(!ConvertToSegmentLocal(lx, ly, lz)){
        return NULL;
    }

    switch (direction) {
    case eUp:
        ly--;
        break;
    case eDown:
        ly++;
        break;
    case eLeft:
        lx--;
        break;
    case eRight:
        lx++;
        break;
    case eAbove:
        lz++;
        break;
    case eBelow:
        lz--;
        break;
    case eUpLeft:
        ly--;
        lx--;
        break;
    case eUpRight:
        ly--;
        lx++;
        break;
    case eDownLeft:
        ly++;
        lx--;
        break;
    case eDownRight:
        ly++;
        lx++;
        break;
    }

    return getTileLocal(lx, ly, lz);
}

Tile* WorldSegment::getTileRelativeTo(auto x, auto y, auto z,  dirRelative direction, int distance)
{
    auto lx = x;
    auto ly = y;
    auto lz = z;

    if(!ConvertToSegmentLocal(lx, ly, lz)){
        return NULL;
    }

    switch (direction) {
    case eUp:
        ly-= distance;
        break;
    case eDown:
        ly+= distance;
        break;
    case eLeft:
        lx-= distance;
        break;
    case eRight:
        lx+= distance;
        break;
    case eAbove:
        lz+= distance;
        break;
    case eBelow:
        lz-= distance;
        break;
    case eUpLeft:
        ly-= distance;
        lx-= distance;
        break;
    case eUpRight:
        ly-= distance;
        lx+= distance;
        break;
    case eDownLeft:
        ly+= distance;
        lx-= distance;
        break;
    case eDownRight:
        ly+= distance;
        lx+= distance;
        break;
    }

    return getTileLocal(lx, ly, lz);
}

Tile* WorldSegment::getTileLocal(auto x, auto y, auto z)
{
    if(x < 0 || x >= segState.Size.x) {
        return 0;
    }
    if(y < 0 || y >= segState.Size.y) {
        return 0;
    }
    if(z < 0 || z >= segState.Size.z) {
        return 0;
    }

    uint32_t index = ConvertLocalToIndex(x, y, z);

    return getTile(index);
}

Tile* WorldSegment::getTile(uint32_t index)
{
    if(index>=getNumTiles() ) {
        return NULL;
    }
    return tiles[index].IsValid() ? &(tiles[index]) : NULL;
}

void WorldSegment::DrawAllTiles()
{
    if(!loaded) {
        return;
    }

    auto& ssConfig = stonesenseState.ssConfig;

    if (ssConfig.config.show_osd) {
        DrawCurrentLevelOutline(true);
    }

    if(todraw.size()>0) {
        al_hold_bitmap_drawing(true);

        int extrude = ssConfig.config.extrude_tiles;
        auto DrawBitmap = [&](ALLEGRO_BITMAP* b, draw_event& todraw) {
            al_draw_tinted_scaled_bitmap(
                b,
                todraw.tint,
                todraw.sx,
                todraw.sy,
                todraw.sw,
                todraw.sh,
                todraw.dx - extrude,
                todraw.dy - extrude,
                todraw.dw + (extrude*2),
                todraw.dh + (extrude*2),
                todraw.flags);
            };

        for(size_t i=0; i<todraw.size(); i++) {
            if(i%ssConfig.config.bitmapHolds==0) {
                al_hold_bitmap_drawing(false);
                al_hold_bitmap_drawing(true);
            }
            switch(todraw[i].type) {
            case Fog:
                al_draw_filled_rectangle(
                        todraw[i].dx,
                        todraw[i].dy,
                        todraw[i].dx + todraw[i].dw,
                        todraw[i].dy + todraw[i].dh,
                        premultiply(ssConfig.config.fogcol)
                    );
                break;
            case TintedScaledBitmap:
                DrawBitmap(std::get<ALLEGRO_BITMAP*>(todraw[i].drawobject), todraw[i]);
                break;
            case CreatureText:
                DrawCreatureText(
                    todraw[i].dx,
                    todraw[i].dy,
                    std::get<Stonesense_Unit*>(todraw[i].drawobject));
                break;
            }
        }
    }

    if (ssConfig.config.show_osd) {
        DrawCurrentLevelOutline(false);
    }

    al_hold_bitmap_drawing(false);
}

/**
 * Assembles sprites for all tiles in the segment.
 * The draw order used draws tiles on a per-block basis, so blocks
 * in the back are drawn before blocks in the front.
 */
void WorldSegment::AssembleAllTiles()
{
    if(!loaded) {
        return;
    }

    clock_t starttime = clock();

    auto& ssState = stonesenseState.ssState;

    // x,y,z print prices
    auto vsxmax = segState.Size.x-1;
    auto vsymax = segState.Size.y-1;
    auto vszmax = segState.Size.z-1; // grabbing one tile +z more than we should for tile rules
    for(auto vsz=0; vsz < vszmax; vsz++) {
        //add the fog to the queue
        if(stonesenseState.ssConfig.config.fogenable) {
            draw_event d = {
                Fog,
                std::monostate{},
                al_map_rgb(255,255,255),
                0,
                0,
                (float)ssState.ScreenW,
                (float)ssState.ScreenH,
                0,
                0,
                (float)ssState.ScreenW,
                (float)ssState.ScreenH,
                0};
            AssembleSprite(d);
        }
        //add the tiles to the queue
        for(auto vsx=1; vsx < vsxmax; vsx++) {
            for(auto vsy=1; vsy < vsymax; vsy++) {
                Tile *b = getTileLocal(vsx,vsy,vsz);
                if (b) {
                    b->AssembleTile();
                }
            }
        }
    }

    stonesenseState.stoneSenseTimers.assembly_time.update(clock() - starttime);
}


bool WorldSegment::CoordinateInsideSegment(auto x, auto y, auto z)
{
    auto lx = x;
    auto ly = y;
    auto lz = z;

    if(!ConvertToSegmentLocal(lx, ly, lz)){
        return NULL;
    }

    if(lx < 0 || lx >= segState.Size.x) {
        return 0;
    }
    if(ly < 0 || ly >= segState.Size.y) {
        return 0;
    }
    if(lz < 0 || lz >= segState.Size.z) {
        return 0;
    }

    return true;
}

bool WorldSegment::RangeInsideSegment(auto min_x, auto min_y, auto min_z, auto max_x, auto max_y, auto max_z)
{
    auto lnx = min_x;
    auto lny = min_y;
    auto lnz = min_z;
    auto lxx = max_x;
    auto lxy = max_y;
    auto lxz = max_z;

    ConvertToSegmentLocal(lnx, lny, lnz);
    ConvertToSegmentLocal(lxx, lxy, lxz);


    if (lxx < 0 || lnx >= segState.Size.x) {
        return 0;
    }
    if (lxy < 0 || lny >= segState.Size.y) {
        return 0;
    }
    if (lxz < 0 || lnz >= segState.Size.z) {
        return 0;
    }

    return true;
}

bool WorldSegment::CoordinateInteriorSegment(auto x, auto y, auto z, uint32_t shellthick)
{
    auto lx = x;
    auto ly = y;
    auto lz = z;

    if(!ConvertToSegmentLocal(lx, ly, lz)){
        return NULL;
    }

    if(lx < 0  + int16_t(shellthick) || lx >= int16_t(segState.Size.x - shellthick)) {
        return 0;
    }
    if(ly < 0  + int16_t(shellthick) || ly >= int16_t(segState.Size.y - shellthick)) {
        return 0;
    }
    if(lz < 0 /*bottom is "interior"*/ || lz >= int16_t(segState.Size.z - shellthick)) {
        return 0;
    }
    return true;
}

void WorldSegment::AssembleSprite(draw_event d)
{
    todraw.push_back(d);
}

void WorldSegment::PushBuilding( std::unique_ptr<Stonesense_Building> building)
{
    buildings.push_back(std::move(building));
}

void WorldSegment::ClearBuildings()
{
    buildings.clear();
}

void WorldSegment::PushUnit( std::unique_ptr<Stonesense_Unit> unit)
{
    units.push_back(std::move(unit));
}

void WorldSegment::ClearUnits()
{
    units.clear();
}
