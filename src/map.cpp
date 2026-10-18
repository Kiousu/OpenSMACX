/*
 * OpenSMACX - an open source clone of Sid Meier's Alpha Centauri.
 * Copyright (C) 2013-2021 Brendan Casey
 *
 * OpenSMACX is free software: you can redistribute it and / or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenSMACX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenSMACX. If not, see <http://www.gnu.org/licenses/>.
 */
#include "stdafx.h"
#include "temp.h"
#include "map.h"
#include "alpha.h"
#include "base.h"
#include "faction.h"
#include "game.h"
#include "path.h"
#include "random.h"
#include "strings.h"
#include "technology.h"
#include "terraforming.h"
#include "veh.h"
#include "log.h"

int *MapLongitudeBounds = (int *)0x00949870;
int *MapLatitudeBounds = (int *)0x00949874;
uint32_t *MapRandSeed = (uint32_t *)0x00949878;
int *MapSeaLevel = (int *)0x0094987C;
int *MapSeaLevelCouncil = (int *)0x00949880;
uint32_t *MapArea = (uint32_t *)0x00949884;
uint32_t *MapAreaSqRoot = (uint32_t *)0x00949888;
BOOL *MapIsFlat = (BOOL *)0x0094988C;
int *MapLandmarkCount = (int *)0x00949890;
Landmark *MapLandmark = (Landmark *)0x00949894; // [64]
MapWin **MapWinList = (MapWin **)0x007D3C3C;
MapWin *MapWinUnk = (MapWin *)0x008EB48C;
MapWin *MapWinUnk2 = (MapWin *)0x006C5C9C;
MapWin *BaseMapWin = (MapWin *)0x007D4060;
uint8_t *LandmarkAltIndex = (uint8_t *)0x00685684;
uint8_t *LandmarkAltCrater = (uint8_t *)0x007F65D0;
uint8_t *LandmarkAltVolcano = (uint8_t *)0x007D3B74;
uint8_t *LandmarkAltCanyon = (uint8_t *)0x007F655C;
uint8_t *LandmarkAltMesa = (uint8_t *)0x007D3A78;
uint8_t *LandmarkAltRidge = (uint8_t *)0x007F64E8;
uint8_t *LandmarkAltCraterDefault = (uint8_t *)0x006857F0;
uint8_t *LandmarkAltVolcanoDefault = (uint8_t *)0x0068577C;
uint8_t *LandmarkAltCanyonDefault = (uint8_t *)0x0068593C;
uint8_t *LandmarkAltMesaDefault = (uint8_t *)0x00685864;
uint8_t *LandmarkAltRidgeDefault = (uint8_t *)0x006859B0;
static_assert(offsetof(MapWin, draw_toggle_a) == 0x1DD74, "MapWin layout");
uint32_t *MapAbstractLongBounds = (uint32_t *)0x0094A294;
uint32_t *MapAbstractLatBounds = (uint32_t *)0x0094A298;
uint32_t *MapAbstractArea = (uint32_t *)0x0094A29C;
uint32_t *MapSizePlanet = (uint32_t *)0x0094A2A0;
uint32_t *MapOceanCoverage = (uint32_t *)0x0094A2A4;
uint32_t *MapLandCoverage = (uint32_t *)0x0094A2A8;
uint32_t *MapErosiveForces = (uint32_t *)0x0094A2AC;
uint32_t *MapPlanetaryOrbit = (uint32_t *)0x0094A2B0;
uint32_t *MapCloudCover = (uint32_t *)0x0094A2B4;
uint32_t *MapNativeLifeForms = (uint32_t *)0x0094A2B8;
LPSTR *MapFilePath = (LPSTR *)0x0094A2BC;
Map **MapTiles = (Map **)0x0094A30C;
uint8_t **MapAbstract = (uint8_t **)0x0094A310;
int *MapBaseSubmergedCount = (int *)0x009B2290; // [8]; reset each time global alt changes
int *MapBaseIdClosestSubmergedVeh = (int *)0x009B22BC; // [8]; reset each time global alt changes
uint32_t *BrushVal1 = (uint32_t *)0x009B22B0;
uint32_t *BrushVal2 = (uint32_t *)0x009B22B8;
uint32_t *WorldBuildVal1 = (uint32_t *)0x009B22B4;
int *MapRainfallChangeBaseID = (int *)0x009B22E0; // set by world_rainfall(): base of the faction at the
                                                  // controls within two tiles of a rainfall change
int *MapRainfallChangeFactionID = (int *)0x009B22DC; // owner of an AI base on a tile that got drier
BOOL *WorldBuildUnk1 = (BOOL *)0x009B22E8; // set for the whole of world_build(); world_shorelines()
                                           // redraws tiles while it is set
BOOL *WorldBuildUnk2 = (BOOL *)0x009B22EC; // set for the whole of world_build(); alt_set() skips base and
                                           // unit effects and world_climate() skips reset_territory()
uint32_t *TerrainBitsFungusClears = (uint32_t *)0x006919EC; // improvements fungus removes: mine,
                                                            // solar/tidal, farm, soil enricher, forest

Continent *Continents = (Continent *)0x009AA730; // [128]
RulesNatural *Natural = (RulesNatural *)0x0094ADE0;
uint32_t *MapLongitude = (uint32_t *)0x0068FAF0; // default set to 1
uint32_t *AltNatural = (uint32_t *)0x0068FB4C;
uint32_t *AltNaturalDefault = (uint32_t *)0x0068FAF4; // constant copy of the AltNatural defaults
LPCSTR MapExtension = "MP";

/*
Purpose: Check whether the coordinates are on the map.
Original Offset: 004712A0
Return Value: Are the coordinates on the map? true/false
Status: Complete
*/
BOOL __cdecl on_map(int x, int y) {
    return y >= 0 && y < (int)*MapLatitudeBounds && x >= 0 && x < (int)*MapLongitudeBounds;
}

/*
Purpose: Bounds check and handling of x coordinate for round maps.
Original Offset: 0048BEE0
Return Value: X coordinate
Status: Complete
*/
int __cdecl xrange(int x) {
    if (!*MapIsFlat) {
        if (x >= 0) {
            if (x >= (int)*MapLongitudeBounds) {
                x -= *MapLongitudeBounds;
            }
        } else {
            x += *MapLongitudeBounds;
        }
    }
    return x;
}

/*
Purpose: Check who owns a tile. Optional parameter to get closest base.
Original Offset: 004E3EF0
Return Value: Faction id of the territory's owner or -1 if no owner or unknown faction
Status: Complete
*/
int __cdecl whose_territory(uint32_t faction_id, uint32_t x, uint32_t y, int *base_id, 
                            BOOL ignore_comm) {
    int owner = map_loc(x, y)->territory;
    if (owner <= 0) {
        return -1; // no owner
    }
    if (faction_id != (uint32_t)owner) {
        if (!ignore_comm && !(*GameState & STATE_OMNISCIENT_VIEW)
            && has_treaty(faction_id, owner, DTREATY_COMMLINK | DTREATY_UNK_8000000)
                != (DTREATY_COMMLINK | DTREATY_UNK_8000000)) {
            return -1; // owner unknown to faction
        }
        if (base_id) {
            *base_id = base_find(x, y, -1, region_at(x, y), -1, -1);
        }
    }
    return owner;
}

/*
Purpose: Find closest base to territory owned by another faction not at war with specified faction.
Original Offset: 004E3FA0
Return Value: Base id or -1
Status: Complete
*/
int __cdecl base_territory(uint32_t faction_id, uint32_t x, uint32_t y) {
    int base_id;
    int owner = whose_territory(faction_id, x, y, &base_id, false);
    if (owner >= 0 && (uint32_t)owner != faction_id && (is_human(faction_id) || is_human(owner))
        && !has_treaty(faction_id, owner, DTREATY_VENDETTA)) {
        return base_id;
    }
    return -1;
}

/*
Purpose: For the specified tile, calculate the quality of the terrain.
Original Offset: 004ECB90
Return Value: Quality of terrain, lower is better (0-2)
Status: Complete
*/
uint32_t __cdecl crappy(uint32_t x, uint32_t y) {
    uint32_t poor_quality = 0;
    uint32_t rainfall = climate_at(x, y);
    if (rainfall == RAINFALL_ARID) {
        poor_quality = 1; // neither moist or rainy
    }
    uint32_t rocky = rocky_at(x, y);
    if (rocky == TERRAIN_BIT_ROCKY) {
        poor_quality++; // rocky
    } else if (rocky == ROCKINESS_FLAT && rainfall < RAINFALL_RAINY) {
        poor_quality++; // flat, moist or arid
    }
    return poor_quality;
}

/*
Purpose: Take the absolute distance between two points as parameters to calculate how far out they
         radiate. This is mainly used to determine proximity or how far away the two points are from
         each other in a rough circle shape (see RadiusOffsetX[]/RadiusOffsetY[]).
Original Offset: 004F8090
Return Value: Distance radius
Status: Complete
*/
int __cdecl vector_dist(int x_distance, int y_distance) {
    x_distance = abs(x_distance);
    y_distance = abs(y_distance);
    int largest = x_distance;
    if (x_distance <= y_distance) {
        largest = y_distance;
    }
    int smallest = x_distance;
    if (x_distance >= y_distance) {
        smallest = y_distance;
    }
    return largest - (((y_distance + x_distance) / 2) - smallest + 1) / 2;
}

/*
Purpose: Take two points and calculate how far out they radiate. This is mainly used to determine
         proximity or how far away the two points are from each other in a rough circle shape.
Original Offset: 005A5910
Return Value: Distance radius
Status: Complete
*/
int __cdecl vector_dist(int x_point_a, int y_point_a, int x_point_b, int y_point_b) {
    return vector_dist(x_dist(x_point_a, x_point_b), abs(y_point_a - y_point_b));
}

/*
Purpose: Check whether there is a path between two regions. It seems to only take into account land
         destinations. TODO: Revisit in the future when Continent/Path is complete.
Original Offset: 0050DDC0
Return Value: Is there a path? true/false
Status: Complete
*/
BOOL __cdecl sea_coast(uint32_t region_dst, uint32_t region_src) {
    uint32_t offset;
    uint32_t mask;
    bitmask(region_src & RegionBounds, &offset, &mask);
    return (Continents[region_dst].sea_coasts[offset] & mask) != 0;
}

/*
Purpose: Count the number of paths from the source region. It seems to only take into account land 
         source and destination ranges. TODO: Revisit in the future when Continent/Path is complete.
Original Offset: 0050DE00
Return Value: Sea coasts valid path count
Status: Complete
*/
uint32_t __cdecl sea_coasts(uint32_t region_src) {
    uint32_t sea_coast_count = 0;
    for (int i = 1; i < RegionBounds; i++) {
        if (sea_coast(i, region_src)) {
            sea_coast_count++;
        }
    }
    return sea_coast_count;
}

/*
Purpose: Check to see whether base is within a one tile radius of a sea tile with specified region.
         If you pass a land region (<63) as the 2nd parameter, it is possible to get collision
         behavior due to region bounding. TODO: Revisit in the future to see whether to remove them.
Original Offset: 0050DE50
Return Value: Is base connected to specified sea region? true/false
Status: Complete
*/
BOOL __cdecl base_on_sea(uint32_t base_id, uint32_t region_sea) {
    region_sea &= RegionBounds;
    if (region_sea >= RegionBounds) { // change to equals since already bounded?
        return false; // skips poles (land or ocean)
    }
    int x = Bases[base_id].x;
    int y = Bases[base_id].y;
    for (uint32_t i = 0; i < 8; i++) {
        int x_radius = xrange(x + RadiusBaseX[i]);
        int y_radius = y + RadiusBaseY[i];
        if (on_map(x_radius, y_radius) && is_ocean(x_radius, y_radius)
            && (region_at(x_radius, y_radius) & RegionBounds) == region_sea) {
            return true;
        }
    }
    return false;
}

/*
Purpose: Determine the ocean region for coastal bases. There is an issue if a base is connected to
         more than one ocean region, it will only return the last checked region based on the base
         radius clockwise order. This could cause the AI to make incorrect calculations for what to
         prioritize building at the base. An example could be a base that is connected to a small
         water body as well as the much larger ocean. Depending on where these bodies of water are
         positioned, the AI might make assumptions not to prioritize building naval units. Also,
         the Continents compare logic isn't used by anything. This might be the root cause of
         outlined bug. TODO: Revisit in the future once more is known about Continent structure.
Original Offset: 0050DF30
Return Value: Ocean region or -1 if landlocked
Status: Complete
*/
int __cdecl base_coast(uint32_t base_id) {
    int region = -1;
    int val = 0;
    int x = Bases[base_id].x;
    int y = Bases[base_id].y;
    for (uint32_t i = 0; i < 8; i++) { // is_coast()
        int x_radius = xrange(x + RadiusBaseX[i]);
        int y_radius = y + RadiusBaseY[i];
        if (on_map(x_radius, y_radius) && is_ocean(x_radius, y_radius)) {
            region = region_at(x_radius, y_radius);
            int compare = (region >= 127) ? 1 : Continents[region].tile_count;
            if (compare >= val) {
                val = compare; // value isn't used?
            }
            i += (2 - (i & 1)); // skips adjacent tiles
        }
    }
    return region;
}

/*
Purpose: Check to see if a port base shares a common body of water with destination coastal region.
Original Offset: 0050E030
Return Value: Is port and coastal region accessible by water to each other? true/false
Status: Complete
*/
BOOL __cdecl port_to_coast(uint32_t base_id, uint32_t region) {
    int x = Bases[base_id].x;
    int y = Bases[base_id].y;
    if (region_at(x, y) == region) {
        return true;
    }
    for (uint32_t i = 0; i < 8; i++) { // is_coast()
        int x_radius = xrange(x + RadiusBaseX[i]);
        int y_radius = y + RadiusBaseY[i];
        if (on_map(x_radius, y_radius) && is_ocean(x_radius, y_radius)) {
            if (sea_coast(region, region_at(x_radius, y_radius))) {
                return true;
            }
            i += (2 - (i & 1)); // skips adjacent tiles
        }
    }
    return false;
}

/*
Purpose: Check to see if two port bases share a common body of water determined by region.
Original Offset: 0050E160
Return Value: Are both ports accessible by water to each other? true/false
Status: Complete
*/
BOOL __cdecl port_to_port(uint32_t base_id_src, uint32_t base_id_dst) {
    int x = Bases[base_id_src].x;
    int y = Bases[base_id_src].y;
    int last_region = -1;
    for (uint32_t i = 0; i < 8; i++) { // is_coast()
        int x_radius = xrange(x + RadiusBaseX[i]);
        int y_radius = y + RadiusBaseY[i];
        if (on_map(x_radius, y_radius) && is_ocean(x_radius, y_radius)) {
            int region_src = region_at(x_radius, y_radius);
            if (region_src != last_region) { // reduce redundant checks especially sea bases
                last_region = region_src;
                if (base_on_sea(base_id_dst, region_src)) {
                    return true;
                }
            }
        }
    }
    return false;
}

/*
Purpose: Determine if a base has access to ports or more than one coastal region. This helps
         prioritize whether naval transports should be built.
Original Offset: 0050E310
Return Value: Should base build naval transports? true/false
Status: Complete
*/
BOOL __cdecl transport_base(uint32_t base_id) {
    int region = base_coast(base_id);
    if (region < 0) {
        return false;
    }
    if (is_ocean(Bases[base_id].x, Bases[base_id].y)) {
        return true;
    }
    return (sea_coasts(region) > 1);
}

/*
Purpose: Determine if there are other faction's ports in the vicinity of the specified base.
Original Offset: 0050E3C0
Return Value: Does base have a strategic naval importance? true/false
Status: Complete
*/
BOOL __cdecl naval_base(uint32_t base_id) {
    if (base_coast(base_id) < 0 || *BaseCurrentCount <= 0) {
        return false;
    }
    uint32_t faction_id = Bases[base_id].faction_id_current;
    for (int i = 0; i < *BaseCurrentCount; i++) {
        if (faction_id != Bases[i].faction_id_current) {
            if (port_to_port(base_id, i)) {
                return true;
            }
        }
    }
    return false;
}

/*
Purpose: Determine if specified unit can set up a convoy route with specified base.
Original Offset: 0050E5C0
Return Value: Is a convoy route possible? true/false
Status: Complete
*/
BOOL __cdecl convoy(uint32_t veh_id, uint32_t base_id) {
    int home_base_id = Vehs[veh_id].home_base_id;
    if (home_base_id < 0 || base_id == (uint32_t)home_base_id) {
        return false;
    }
    uint32_t triad = get_triad(veh_id);
    if (triad == TRIAD_AIR) {
        return true;
    }
    uint32_t region_base = region_at(Bases[base_id].x, Bases[base_id].y);
    if (region_at(Bases[home_base_id].x, Bases[home_base_id].y) == region_base
        && ((region_base >= 64) == (triad == TRIAD_SEA))) {
        return true; // same region, by land or sea
    }
    if (triad) {
        return port_to_port(base_id, home_base_id); // sea
    }
    return false;
}

/*
Purpose: Validate region bounds. Bad regions include: 0, 63, 64, 127, 128.
Original Offset: 005591C0
Return Value: Is region bad? true/false
Status: Complete
*/
BOOL __cdecl bad_reg(int region) {
    return (region & RegionBounds) == RegionBounds || !(region & RegionBounds);
}

/*
Purpose: Check whether a tile is a good place for a sensor array: inside the faction's territory,
         not fungus it cannot improve, no sensor of its own within the 25-tile radius, and either
         not fully covered by its bases' sensors or at the territory's edge; the tile itself must
         be empty of bonuses, bases, boreholes, condensers, monoliths, solar collectors and
         mines, and be rocky, arid or fungus.
Original Offset: 00564EB0
Return Value: Is the tile a good sensor site? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch8.log)
*/
BOOL __cdecl good_sensor(uint32_t faction_id, int x, int y) {
    if (whose_territory(faction_id, x, y, NULL, false) != (int)faction_id) {
        return false;
    }
    int lat = *MapLatitudeBounds, lon = *MapLongitudeBounds;
    Map *tile = map_loc(x, y);
    if ((tile->bit & BIT_FUNGUS) && (tile->climate & 0xE0) >= ALT_BIT_OCEAN_SHELF
        && !has_tech(Rules->tech_improve_fungus_sqr, faction_id)) {
        return false;
    }
    uint32_t status = 0;
    for (int i = 0; i < 25; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (y_radius < 0 || y_radius >= lat || x_radius < 0 || x_radius >= lon) {
            continue;
        }
        if (is_sensor(x_radius, y_radius)
            && whose_territory(faction_id, x_radius, y_radius, NULL, false) == (int)faction_id) {
            return false;
        }
        if (i < 9) {
            int base_id = base_at(x_radius, y_radius);
            if (base_id >= 0 && Bases[base_id].faction_id_current == faction_id) {
                BOOL covered = false;
                for (int j = 0; j < 25; j++) {
                    int x_sensor = xrange(x_radius + RadiusOffsetX[j]);
                    int y_sensor = y_radius + RadiusOffsetY[j];
                    if (y_sensor >= 0 && y_sensor < lat && x_sensor >= 0 && x_sensor < lon
                        && is_sensor(x_sensor, y_sensor)
                        && whose_territory(faction_id, x_sensor, y_sensor, NULL, false)
                            == (int)faction_id) {
                        covered = true;
                        break;
                    }
                }
                status |= covered ? 2 : 6;
            }
            if (whose_territory(faction_id, x_radius, y_radius, NULL, false) != (int)faction_id) {
                status |= 1;
            }
        }
    }
    if (!status || bonus_at(x, y, 0)) {
        return false;
    }
    if ((tile->bit & BIT_BASE_IN_TILE) && (tile->val2 & 0xF) <= 7) {
        return false;
    }
    uint32_t bit = tile->bit;
    if (bit & (BIT_THERMAL_BORE | BIT_CONDENSER | BIT_MONOLITH | BIT_SOLAR_TIDAL | BIT_MINE)) {
        return false;
    }
    return (tile->val3 & 0xC0) > 0x40 || !(tile->climate & 0x18)
        || ((bit & BIT_FUNGUS) && (tile->climate & 0xE0) >= ALT_BIT_OCEAN_SHELF);
}

/*
Purpose: Pick the terraforming order an AI former (or the governor, with is_base_work set) should
         carry out on a tile, for the faction and optionally for the base whose radius it's in.
         Ocean: remove sea fungus, raise the sea floor, a kelp farm, a mining platform or a tidal
         harness by the base's needs; land: remove fungus, then a solar collector, mine or farm
         (soil enricher) weighing rainfall, rockiness, elevation, bonuses and the base's needs, and
         finally a forest or, for an AI, fungus when the tile's value stays below what those yield.
         Tiles in the territory of a faction without a pact are left alone.
Original Offset: 00565320
Return Value: Order (ORDER_FARM, ORDER_MINE, ORDER_SOLAR_COLLECTOR, ORDER_PLANT_FOREST,
              ORDER_REMOVE_FUNGUS, ORDER_PLANT_FUNGUS, ORDER_TERRAFORM_UP) or 0 for nothing
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch24.log
        and 2026-10-16-sweep-batch24.log)
*/
int __cdecl can_terraform(int faction_id, int x, int y, BOOL is_base_work, int base_id) {
    Map *tile = map_loc(x, y);
    if (tile->bit & BIT_BASE_IN_TILE && (tile->val2 & 0xF) < 8) {
        return 0;
    }
    uint32_t bit = tile->bit;
    BOOL fungus = bit & BIT_FUNGUS && (tile->climate & 0xE0) >= ALT_BIT_OCEAN_SHELF;
    uint32_t mine = bit & BIT_MINE;
    uint32_t solar = bit & BIT_SOLAR_TIDAL;
    uint32_t farm = bit & BIT_FARM;
    // a condenser, echelon mirror or borehole, or the volcano's crater
    BOOL major = (bit & (BIT_CONDENSER | BIT_ECH_MIRROR | BIT_THERMAL_BORE))
        || ((tile->bit2 & (BIT2_UNK_80000000 | BIT2_VOLCANO)) == BIT2_VOLCANO
            && !(tile->bit2 & 0xFF000000));
    PlayerData *player = &PlayersData[faction_id];
    int fungus_yield = player->tech_fungus_energy + player->tech_fungus_mineral
        + player->tech_fungus_nutrient;
    int forest_yield = ResourceInfo[RSCINFO_FOREST_SQ].energy
        + ResourceInfo[RSCINFO_FOREST_SQ].minerals + ResourceInfo[RSCINFO_FOREST_SQ].nutrients;
    if (base_id >= 0) {
        if (has_fac_built(FAC_TREE_FARM, base_id)) {
            forest_yield++;
        }
        if (has_fac_built(FAC_HYBRID_FOREST, base_id)) {
            forest_yield += 2;
        }
    }
    BOOL human = *FactionsStatus & (1 << faction_id);
    if (player->ai_growth && !player->ai_power && !player->ai_wealth && !player->ai_tech
        && player->ai_fight < 0 && !human) {
        forest_yield++; // a builder AI likes forests
    }
    uint8_t climate = tile->climate;
    uint32_t altitude = climate & 0xE0;
    if (altitude < ALT_BIT_SHORE_LINE) {
        if (fungus) {
            if (fungus_yield > 3 || (human && !(*GameMorePreferences & 8))) {
                return 0;
            }
            return has_tech(Terraforming[TERRA_REMOVE_FUNGUS].preq_tech_sea, faction_id)
                ? ORDER_REMOVE_FUNGUS : 0;
        }
        if (altitude < ALT_BIT_OCEAN_SHELF) {
            return terrain_avail(TERRA_RAISE_LAND, true, faction_id) ? ORDER_TERRAFORM_UP : 0;
        }
        if (tile->bit & BIT_THERMAL_BORE) {
            return 0;
        }
        if (!farm && has_tech(Terraforming[TERRA_FARM].preq_tech_sea, faction_id)) {
            return ORDER_FARM;
        }
        if (!solar && !mine && !major) {
            bonus_at(x, y, 0); // result unused in the original
            if (base_id >= 0) {
                uint32_t state = Bases[base_id].state;
                if (state & BSTATE_UNK_2000
                    && has_tech(Terraforming[TERRA_MINE].preq_tech_sea, faction_id)) {
                    return ORDER_MINE;
                }
                if (state & BSTATE_UNK_1000
                    && has_tech(Terraforming[TERRA_SOLAR_TIDAL].preq_tech_sea, faction_id)) {
                    return ORDER_SOLAR_COLLECTOR;
                }
                if ((state & BSTATE_UNK_4000 || Bases[base_id].nutrient_surplus < 2)
                    && has_tech(Terraforming[TERRA_SOLAR_TIDAL].preq_tech_sea, faction_id)
                    && Rules->tgl_nutrient_effect_with_mine < 0) {
                    return ORDER_SOLAR_COLLECTOR;
                }
                // weigh the base's sea mines against its tidal harnesses
                int harnesses = 0;
                int mines = Rules->tgl_nutrient_effect_with_mine < 0;
                for (int i = 0; i < BaseRadiusTiles; i++) {
                    int x2 = RadiusOffsetX[i] + Bases[base_id].x;
                    if (!(*MapIsFlat & 1)) {
                        if (x2 < 0) {
                            x2 += *MapLongitudeBounds;
                        } else if (x2 >= *MapLongitudeBounds) {
                            x2 -= *MapLongitudeBounds;
                        }
                    }
                    int y2 = RadiusOffsetY[i] + Bases[base_id].y;
                    if (y2 >= 0 && y2 < *MapLatitudeBounds && x2 >= 0 && x2 < *MapLongitudeBounds
                        && (map_loc(x2, y2)->climate & 0xE0) < ALT_BIT_SHORE_LINE) {
                        uint32_t bit2 = map_loc(x2, y2)->bit;
                        if (bit2 & BIT_MINE) {
                            mines++;
                        }
                        if (bit2 & BIT_SOLAR_TIDAL) {
                            harnesses++;
                        }
                    }
                }
                if (harnesses < !has_tech(Terraforming[TERRA_MINE].preq_tech_sea, faction_id) + mines
                    && has_tech(Terraforming[TERRA_SOLAR_TIDAL].preq_tech_sea, faction_id)) {
                    return ORDER_SOLAR_COLLECTOR;
                }
            }
            if (has_tech(Terraforming[TERRA_MINE].preq_tech_sea, faction_id)
                && (Rules->tgl_nutrient_effect_with_mine >= 0
                    || (base_id >= 0 && Bases[base_id].nutrient_surplus > 1
                        && !(Bases[base_id].state & BSTATE_UNK_4000)))) {
                return ORDER_MINE;
            }
        }
        if (farm && !(bit & BIT_SOIL_ENRICHER)
            && has_tech(Terraforming[TERRA_SOIL_ENR].preq_tech_sea, faction_id)) {
            return ORDER_FARM; // a farmed tile gets the enricher
        }
        return 0;
    }
    int rainfall = (climate & 0x18) >> 3;
    uint32_t rocky = tile->val3 >> 6;
    int order = 0;
    int crop = crop_yield(faction_id, -1, x, y, false);
    int elevation = (climate >> 5) - 2;
    if (elevation < 0) {
        elevation = 0;
    }
    int bonus = bonus_at(x, y, 0);
    int value = elevation + rainfall + 1 + rocky; // what the tile is worth as it is
    int value_kept = value;
    if (has_tech(Terraforming[TERRA_SOIL_ENR].preq_tech, faction_id) && farm) {
        value_kept++;
    }
    int owner = whose_territory(faction_id, x, y, NULL, false);
    if (owner >= 0 && owner != faction_id
        && !(PlayersData[faction_id].diplo_treaties[owner] & DTREATY_PACT)) {
        return 0;
    }
    if (fungus) {
        if (base_id >= 0 && Bases[base_id].eco_damage / 2 + value_kept <= fungus_yield) {
            return 0;
        }
        if (human && !(*GameMorePreferences & 8)) {
            return 0;
        }
        // the original checks the sea fungus tech here too
        return has_tech(Terraforming[TERRA_REMOVE_FUNGUS].preq_tech_sea, faction_id)
            ? ORDER_REMOVE_FUNGUS : 0;
    }
    if (tile->bit & BIT_MONOLITH) {
        return 0;
    }
    if (bit & BIT_FOREST && value_kept <= forest_yield) {
        return 0;
    }
    BOOL mine_tech = has_tech(Terraforming[TERRA_MINE].preq_tech, faction_id);
    BOOL solar_tech = has_tech(Terraforming[TERRA_SOLAR_TIDAL].preq_tech, faction_id);
    has_tech(Terraforming[TERRA_FARM].preq_tech, faction_id); // result unused in the original
    if (!mine && !solar && !major) {
        if (solar_tech && elevation && ((((int)rocky == 1 || elevation > 1)
            && crop >= (farm ? 2 : 1)) || is_base_work)) {
            order = ORDER_SOLAR_COLLECTOR;
        }
        BOOL farm_check = !mine_tech; // the original's label at 0x565B14
        if (mine_tech) {
            if (bonus == 2 || rocky == 2) {
                order = ORDER_MINE;
            }
            // the original's branches at 0x565AA2..0x565AF5
            BOOL rolling_check = false;
            BOOL skip = false; // on to the farm check, past the base work rule
            if (bonus != 2 && rocky == 2) {
                rolling_check = true;
            } else if (bonus != 2 || rocky != 2) {
                int crop_kept = !farm + crop;
                if (Rules->tgl_nutrient_effect_with_mine >= 0 || crop_kept < 2) {
                    rolling_check = true;
                } else if (rocky || crop_kept < 3) {
                    skip = true;
                    farm_check = true;
                }
            }
            if (!skip) {
                if (rolling_check) {
                    if (rocky == 1 && !elevation) {
                        order = ORDER_MINE;
                    }
                    if (bonus == 3) {
                        order = ORDER_MINE;
                    }
                }
                if (is_base_work) {
                    if (farm && !order) {
                        order = ORDER_MINE;
                    }
                    farm_check = true;
                }
            }
        }
        if (farm_check && is_base_work && farm && !order && elevation && solar_tech) {
            order = ORDER_SOLAR_COLLECTOR;
        }
        if (base_id >= 0) {
            uint32_t state = Bases[base_id].state;
            if (state & BSTATE_UNK_1000 && solar_tech && elevation
                && ((farm ? 2 : 1) <= crop || elevation > 1)) {
                order = ORDER_SOLAR_COLLECTOR;
            }
            if (state & BSTATE_UNK_2000 && mine_tech) {
                if ((farm ? 3 : 2) <= crop) {
                    order = ORDER_MINE;
                }
                if (((rocky != 1 && rocky) || bonus == 2
                    || (rainfall && rocky < 2 && elevation < 2)) && (farm || crop > 1)) {
                    order = ORDER_MINE;
                }
            }
        }
    }
    if (has_tech(Terraforming[TERRA_FARM].preq_tech, faction_id) && !farm && rocky < 2) {
        Map *now = map_loc(x, y);
        if (!(now->bit & BIT_THERMAL_BORE) && (now->bit2 & (BIT2_UNK_80000000 | BIT2_VOLCANO)) != BIT2_VOLCANO
            && !(now->bit & BIT_ECH_MIRROR) && (crop < 2
                || has_tech(Rules->tech_three_nutrients_sqr, faction_id) || bonus == 1)) {
            if (solar || mine) {
                order = ORDER_FARM;
            }
            if (base_id >= 0 && Bases[base_id].state & BSTATE_UNK_4000) {
                order = ORDER_FARM;
            } else if (order == ORDER_SOLAR_COLLECTOR && crop < 2) {
                order = ORDER_FARM;
            } else if (!order) {
                order = ORDER_FARM;
            }
            if (rainfall <= 1) {
                order = ORDER_FARM;
            }
        }
    }
    if (has_tech(Terraforming[TERRA_SOIL_ENR].preq_tech, faction_id) && farm
        && !(bit & BIT_SOIL_ENRICHER)) {
        BOOL enrich;
        if (crop < 2 || (crop == 2 && !has_tech(Rules->tech_three_nutrients_sqr, faction_id))) {
            enrich = !order && (is_base_work || crop > 1);
        } else {
            if (solar || mine) {
                order = ORDER_FARM;
            }
            if (base_id >= 0 && Bases[base_id].state & BSTATE_UNK_4000) {
                enrich = true;
            } else {
                enrich = !order && (is_base_work || crop > 1);
            }
        }
        if (enrich) {
            order = ORDER_FARM; // a farmed tile gets the enricher
        }
    }
    if (has_tech(Terraforming[TERRA_SOIL_ENR].preq_tech, faction_id) && farm) {
        value++;
    }
    if (base_id >= 0) {
        uint32_t state = Bases[base_id].state;
        if (state & BSTATE_UNK_4000) {
            if (rainfall > 1) {
                value++;
            }
            if (!rainfall) {
                value--;
            }
        }
        if (state & BSTATE_UNK_1000) {
            if (elevation > 1) {
                value++;
            }
            if (!elevation) {
                value--;
            }
        }
        if (state & BSTATE_UNK_2000 && !rocky) {
            value--;
        }
        if (!(state & (BSTATE_UNK_2000 | BSTATE_UNK_4000)) && solar) {
            value++;
        }
    }
    if (rainfall && elevation && rocky == 1 && *TurnCurrentNum < 50) {
        value++;
    }
    int result = order;
    if (has_tech(Terraforming[TERRA_FOREST].preq_tech, faction_id) && rocky < 2 && !mine
        && !(bit & BIT_FOREST) && (!farm || (!elevation && rainfall < 2 && !human)) && !major
        && (map_loc(x, y)->bit2 & (BIT2_UNK_80000000 | BIT2_VOLCANO)) != BIT2_VOLCANO
        && value < forest_yield
        && (!human || (faction_id == *FactionIDCurrent && *GamePreferences & 0x1000000))) {
        result = ORDER_PLANT_FOREST;
    }
    if (has_tech(Terraforming[TERRA_PLANT_FUNGUS].preq_tech, faction_id) && !human) {
        if (rocky == 2) {
            value += (mine != 0) + 1;
        }
        if (!major && value + 1 < fungus_yield) {
            result = ORDER_PLANT_FUNGUS;
        }
    }
    return result;
}

/*
Purpose: Determine whether specified unit can physically reach the destination coordinates.
Original Offset: 0056B320
Return Value: Can unit reach tile? true/false
Status: Complete
*/
BOOL __cdecl get_there(uint32_t veh_id, uint32_t x_dst, uint32_t y_dst) {
    uint32_t triad = get_triad(veh_id);
    if (triad == TRIAD_AIR) {
        return true;
    }
    int x_src = Vehs[veh_id].x; 
    int y_src = Vehs[veh_id].y;
    uint32_t region_src = region_at(x_src, y_src);
    uint32_t region_dst = region_at(x_dst, y_dst);
    if (!triad) { // TRIAD_LAND
        return (region_src == region_dst);
    }
    int base_id_src = base_at(x_src, y_src);
    int base_id_dst = base_at(x_dst, y_dst);
    if (base_id_dst >= 0) {
        return (base_id_src < 0) ? base_on_sea(base_id_dst, region_src)
            : port_to_port(base_id_src, base_id_dst);
    }
    if (is_ocean(x_dst, y_dst)) {
        return (base_id_src < 0) ? (region_src == region_dst) 
            : base_on_sea(base_id_src, region_dst);
    }
    return (base_id_src < 0) ? sea_coast(region_dst, region_src)
        : port_to_coast(base_id_src, region_dst);
}

/*
Purpose: Determine whether point A is a coast or border tile. It seems that the point B check is
         effectively disabled since usage of this function passes same coordinates for both points.
         Modified return value to boolean rather than returning i. It is always treated as boolean
         and makes a lot more sense than returning the iterator position.
Original Offset: 0056B480
Return Value: Is point A considered a border or coast? true/false
Status: Complete
*/
BOOL __cdecl coast_or_border(uint32_t x_point_a, uint32_t y_point_a, uint32_t x_point_b, 
                             uint32_t y_point_b, uint32_t faction_id) {
    if ((int)faction_id != whose_territory(faction_id, x_point_a, y_point_a, NULL, false)) {
        return false; // faction doesn't control point A
    }
    uint32_t region_a = region_at(x_point_a, y_point_a);
    uint32_t region_b = region_at(x_point_b, y_point_b);
    for (uint32_t i = 1; i < 9; i++) {
        int x_radius = xrange(x_point_a + RadiusOffsetX[i]);
        int y_radius = y_point_a + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius) && (is_ocean(x_radius, y_radius)
                || whose_territory(faction_id, x_radius, y_radius, NULL, false) != (int)faction_id
                || region_a != region_b)) { // not sure about reason for this
            return true; // modified to boolean rather than returning i
        }
    }
    return false;
}

/*
Purpose: Get the map tile for the specified coordinates.
Original Offset: n/a
Return Value: Pointer to map tile
Status: Complete
*/
Map *__cdecl map_loc(uint32_t x, uint32_t y) {
    return &((*MapTiles)[(x >> 1) + y * *MapLongitude]);
}

/*
Purpose: Get the temperature of the specified tile.
Original Offset: n/a
Return Value: Temperature
Status: Complete
*/
uint32_t __cdecl temp_at(uint32_t x, uint32_t y) {
    return map_loc(x, y)->climate & 7;
}

/*
Purpose: Set the temperature for the specified tile.
Original Offset: 00591AD0
Return Value: n/a
Status: Complete
*/
void __cdecl temp_set(uint32_t x, uint32_t y, uint8_t temperature) {
    Map *tile = map_loc(x, y);
    tile->climate &= 0xF8;
    tile->climate |= temperature & 7;
}

/*
Purpose: Get the rainfall (climate) of the specified tile.
Original Offset: n/a
Return Value: Rainfall (climate)
Status: Complete
*/
uint32_t __cdecl climate_at(uint32_t x, uint32_t y) {
    return map_loc(x, y)->climate & (RAINFALL_RAINY | RAINFALL_MOIST);
}

/*
Purpose: Set the rainfall (climate) for the specified tile.
Original Offset: 00591A80
Return Value: n/a
Status: Complete
*/
void __cdecl climate_set(uint32_t x, uint32_t y, uint8_t rainfall) {
    Map *tile = map_loc(x, y);
    tile->climate &= 0xE7;
    tile->climate |= (rainfall & 3) << 3;
    tile->bit2 |= 0x400000; // TODO: identify value
    *UnkBitfield1 |= 1; // TODO: identify global + value
}

/*
Purpose: Calculate the elevation of the specified tile.
Original Offset: 005919C0
Return Value: Elevation (bounded to: -3000 to 3500)
Status: Complete
*/
int __cdecl elev_at(uint32_t x, uint32_t y) {
    uint32_t contour = alt_detail_at(x, y);
    int elev = 50 * (contour - ElevDetail[3] - *MapSeaLevel);
    elev += (contour <= ElevDetail[alt_at(x, y)]) ? 10 
        : (x * 113 + y * 217 + *MapSeaLevel * 301) % 50;
    return range(elev, -3000, 3500);
}

/*
Purpose: Calculate the natural altitude of the specified tile.
Original Offset: 005918A0
Return Value: Natural altitude on a scale from 0 (ocean trench) to 6 (mountain tops)
Status: Complete
*/
uint32_t __cdecl alt_natural(uint32_t x, uint32_t y) {
    int contour = alt_detail_at(x, y) - *MapSeaLevel; // signed: sea level can exceed the contour
    for (uint32_t natural = ALT_3_LEVELS_ABOVE_SEA; natural > 0; natural--) {
        if ((int)AltNatural[natural] <= contour) {
            return natural;
        }
    }
    return 0;
}

/*
Purpose: Set both the altitude and natural altitude for the specified tile. The altitude_natural
         parameter can be between 0 to 9.
Original Offset: 005918F0
Return Value: n/a
Status: Complete
*/
void __cdecl alt_set_both(uint32_t x, uint32_t y, uint32_t altitude_natural) {
    alt_set(x, y, altitude_natural);
    if (alt_natural(x, y) != altitude_natural) {
        alt_put_detail(x, y, (uint8_t)(AltNatural[altitude_natural] + *MapSeaLevel
            + rnd(AltNatural[altitude_natural + 1] - AltNatural[altitude_natural], NULL)));
    }
}

/*
Purpose: Get the bit shifted (down) altitude of the specified tile.
Original Offset: 00500150
Return Value: Altitude
Status: Complete
*/
int __cdecl alt_at(uint32_t x, uint32_t y) {
    return map_loc(x, y)->climate >> 5;
}

/*
Purpose: Get the non-shifted altitude of the specified tile.
Original Offset: n/a
Return Value: Altitude
Status: Complete
*/
int __cdecl altitude_at(uint32_t x, uint32_t y) {
    return map_loc(x, y)->climate & 0xE0;
}

/*
Purpose: Get the altitude details of the specified tile.
Original Offset: 00500180
Return Value: Altitude detail
Status: Complete
*/
int __cdecl alt_detail_at(uint32_t x, uint32_t y) {
    return map_loc(x, y)->contour;
}

/*
Purpose: Set the altitude details for the specified tile.
Original Offset: 00591260
Return Value: n/a
Status: Complete
*/
void __cdecl alt_put_detail(uint32_t x, uint32_t y, uint8_t detail) {
    map_loc(x, y)->contour = detail;
}

/*
Purpose: Get the faction owner of the specified tile.
Original Offset: n/a
Return Value: Faction id
Status: Complete
*/
uint32_t __cdecl owner_at(uint32_t x, uint32_t y) {
    return map_loc(x, y)->val2 & 0xF;
}

/*
Purpose: Set the faction owner for the specified tile.
Original Offset: 00591B10
Return Value: n/a
Status: Complete
*/
void __cdecl owner_set(uint32_t x, uint32_t y, int faction_id) {
    Map *tile = map_loc(x, y);
    tile->val2 &= 0xF0;
    tile->val2 |= faction_id & 0xF;
}

/*
Purpose: Set the site for the specified tile.
Original Offset: 00591B50
Return Value: n/a
Status: Complete
*/
void __cdecl site_set(uint32_t x, uint32_t y, int site) {
    Map *tile = map_loc(x, y);
    tile->val2 &= 0x0F;
    tile->val2 |= site << 4;
}

/*
Purpose: Get the region of the specified tile.
Original Offset: 00500220
Return Value: Region
Status: Complete
*/
uint32_t __cdecl region_at(uint32_t x, uint32_t y) {
    return map_loc(x, y)->region;
}

/*
Purpose: Set the region for the specified tile.
Original Offset: 00591B90
Return Value: n/a
Status: Complete
*/
void __cdecl region_set(uint32_t x, uint32_t y, uint8_t region) {
    map_loc(x, y)->region = region;
}

/*
Purpose: Get the using faction id of the specified tile.
Original Offset: n/a
Return Value: Faction id
Status: Complete
*/
uint32_t __cdecl using_at(uint32_t x, uint32_t y) {
    return map_loc(x, y)->val3 & 7;
}

/*
Purpose: Set the using faction id for the specified tile.
Original Offset: 00591C10
Return Value: n/a
Status: Complete
*/
void __cdecl using_set(uint32_t x, uint32_t y, int faction_id) {
    Map *tile = map_loc(x, y);
    tile->val3 &= 0xF8;
    tile->val3 |= faction_id & 7;
}

/*
Purpose: Get the lock faction id of the specified tile.
Original Offset: n/a
Return Value: Faction id
Status: Complete
*/
uint32_t __cdecl lock_at(uint32_t x, uint32_t y) {
    return (map_loc(x, y)->val3 >> 3) & 7;
}

/*
Purpose: Set the lock faction id for the specified tile.
Original Offset: 00591C50
Return Value: n/a
Status: Complete
*/
void __cdecl lock_set(uint32_t x, uint32_t y, uint32_t faction_id) {
    Map *tile = map_loc(x, y);
    tile->val3 &= 0xC7;
    tile->val3 |= (faction_id & 7) << 3;
}

/*
Purpose: Lock the specified tile for the faction id.
Original Offset: 00591C90
Return Value: True if already locked by another faction, otherwise false
Status: Complete
*/
BOOL __cdecl lock_map(uint32_t x, uint32_t y, uint32_t faction_id) {
    uint32_t lock_id = lock_at(x, y);
    if (lock_id != faction_id) {
        if (lock_id) {
            return true;
        }
        lock_set(x, y, faction_id);
    }
    return false;
}

/*
Purpose: Unlock the specified tile for faction id.
Original Offset: 00591CF0
Return Value: n/a
Status: Complete
*/
void __cdecl unlock_map(uint32_t x, uint32_t y, uint32_t faction_id) {
    if (lock_at(x, y) == faction_id) {
        map_loc(x, y)->val3 &= 0xC7;
    }
}

/*
Purpose: Get the rockiness of the specified tile.
Original Offset: n/a
Return Value: Rockiness
Status: Complete
*/
uint32_t __cdecl rocky_at(uint32_t x, uint32_t y) {
    return map_loc(x, y)->val3 & 0xC0;
}

/*
Purpose: Set the rockiness for the specified tile.
Original Offset: 00591BC0
Return Value: n/a
Status: Complete
*/
void __cdecl rocky_set(uint32_t x, uint32_t y, uint8_t rocky) {
    Map *tile = map_loc(x, y);
    tile->val3 &= 0x3F;
    tile->val3 |= rocky << 6;
    tile->bit2 |= 0x400000; // TODO: identify value
    *UnkBitfield1 |= 1; // TODO: identify variable + value
}

/*
Purpose: Get the bit of the specified tile.
Original Offset: 005001B0
Return Value: Bitfield
Status: Complete
*/
uint32_t __cdecl bit_at(uint32_t x, uint32_t y) {
    return map_loc(x, y)->bit;
}

/*
Purpose: Set the bit for the specified tile.
Original Offset: 00591D30
Return Value: n/a
Status: Complete
*/
void __cdecl bit_put(uint32_t x, uint32_t y, uint32_t bit) {
    map_loc(x, y)->bit = bit;
}

/*
Purpose: Set or unset bit for the specified tile.
Original Offset: 00591D60
Return Value: n/a
Status: Complete
*/
void __cdecl bit_set(uint32_t x, uint32_t y, uint32_t bit, BOOL set) {
    if (set) {
        map_loc(x, y)->bit |= bit;
    } else {
        map_loc(x, y)->bit &= ~bit;
    }
}

/*
Purpose: Get the bit2 of the specified tile.
Original Offset: n/a
Return Value: Bitfield
Status: Complete
*/
uint32_t __cdecl bit2_at(uint32_t x, uint32_t y) {
    return map_loc(x, y)->bit2;
}

/*
Purpose: Set or unset bit2 for the specified tile.
Original Offset: 00591DB0
Return Value: n/a
Status: Complete
*/
void __cdecl bit2_set(uint32_t x, uint32_t y, uint32_t bit2, BOOL set) {
    if (set) {
        map_loc(x, y)->bit2 |= bit2;
    } else {
        map_loc(x, y)->bit2 &= ~bit2;
    }
}

/*
Purpose: Get the code of the specified tile which keeps track of tile sequence order for landmarks.
Original Offset: n/a
Return Value: Code
Status: Complete
*/
uint32_t __cdecl code_at(uint32_t x, uint32_t y) {
    return map_loc(x, y)->bit2 >> 24;
}

/*
Purpose: Set the code for the specified tile which keeps track of tile sequence order for landmarks.
Original Offset: 00591E00
Return Value: n/a
Status: Complete
*/
void __cdecl code_set(uint32_t x, uint32_t y, uint32_t code) {
    Map *tile = map_loc(x, y);
    tile->bit2 &= 0xFFFFFF;
    tile->bit2 |= code << 24;
    *UnkBitfield1 |= 4; // TODO: identify variable + value
}

/*
Purpose: Synchronize the actual tile bit with the faction visible bit.
Original Offset: 00591E50
Return Value: n/a
Status: Complete
*/
void __cdecl synch_bit(uint32_t x, uint32_t y, uint32_t faction_id) {
    if (faction_id) {
        map_loc(x, y)->bit_visible[faction_id - 1] = bit_at(x, y);
    }
}

/*
Purpose: Determine the tile's mineral count that translates to rockiness.
Original Offset: 00591F00
Return Value: 0 (Flat), 1 (Rolling), 2 (Rocky)
Status: Complete
*/
int __cdecl minerals_at(int x, int y) {
    if (!y || y == (*MapLatitudeBounds - 1)) {
        return 2; // poles
    }
    int alt = alt_at(x, y);
    int avg = (x + y) >> 1;
    x -= avg;
    int val1 = (x / 2) + *MapRandSeed + (x - (x % 2)) + (avg - (avg % 2));
    int val2 = (val1 - 2 * (x & 1) - (avg & 1)) & 3;
    int type = abs(alt - ALT_SHORE_LINE);
    if (alt < ALT_SHORE_LINE) {
        type--;
    }
    switch (type) {
      case 0: {
        switch (val2) {
          case 0:
            return 1;
          case 1:
          case 3:
            return 0;
          case 2:
            return ((val1 & 4) != 0) + 1; // 1 or 2
          default:
            return ~val2 & 1;
        }
      }
      case 1:
        return (val2 < 0 || val2 > 2) ? 2 : (val2 == 2) ? 1 : val2;
      case 2:
        return (val2 < 0 || val2 > 1) ? 2 : val2;
      case 3:
        return (val2 < 0 || val2 > 1) ? 2 : 1;
      default:
        return ~val2 & 1;
    }
}

/*
Purpose: Determine if the tile has a resource bonus. While the last parameter is unused, it's set to
         1 by two calls inside world_site(). Otherwise, all other calls have it set to 0.
Original Offset: 00592030
Return Value: 0 (no bonus), 1 (nutrient), 2 (mineral), 3 (energy)
Status: Complete
*/
uint32_t __cdecl bonus_at(uint32_t x, uint32_t y, int UNUSED(unk_val)) {
    uint32_t bit = bit_at(x, y);
    uint32_t alt = alt_at(x, y);
    BOOL has_rsc_bonus = bit & BIT_RSC_BONUS;
    if (!has_rsc_bonus && (!*MapRandSeed
        || (alt >= ALT_SHORE_LINE && !(*GameRules & RULES_NO_UNITY_SCATTERING)))) {
        return 0;
    }
    uint32_t avg = (x + y) >> 1;
    x -= avg;
    uint32_t chk = (avg & 3) + 4 * (x & 3);
    if (!has_rsc_bonus && chk != ((*MapRandSeed + (-5 * (avg >> 2)) - 3 * (x >> 2)) & 0xF)) {
        return 0;
    }
    if (alt < ALT_OCEAN_SHELF) {
        return 0;
    }
    uint32_t ret = (alt < ALT_SHORE_LINE) ? chk % 3 + 1 : (chk % 5) & 3;
    if (!ret || bit & BIT_NUTRIENT_RSC) {
        if (bit & BIT_ENERGY_RSC) {
            return 3; // energy
        }
        return ((bit & BIT_MINERAL_RSC) != 0) + 1; // nutrient or mineral
    }
    return ret;
}

/*
Purpose: Determine if the tile has a supply pod and if so what type.
Original Offset: 00592140
Return Value: 0 (no supply pod), 1 (standard supply pod), 2 (unity pod?)
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-ptrfix.log)
*/
uint32_t __cdecl goody_at(uint32_t x, uint32_t y) {
    uint32_t bit = bit_at(x, y);
    if (bit & (BIT_SUPPLY_REMOVE | BIT_MONOLITH)) {
        return 0; // nothing, supply pod already opened or monolith
    }
    if (*GameRules & RULES_NO_UNITY_SCATTERING) {
        return (bit & (BIT_UNK_4000000 | BIT_UNK_8000000)) ? 2 : 0; // ?
    }
    if (bit & BIT_SUPPLY_POD) {
        return 1; // supply pod
    }
    if (!*MapRandSeed) {
        return 0; // nothing
    }
    uint32_t avg = (x + y) >> 1;
    int x_diff = x - avg;
    uint32_t cmp = (avg & 3) + 4 * (x_diff & 3);
    if (!is_ocean(x, y)
        && cmp == ((-5 * (avg >> 2) - 3 * (x_diff >> 2) + *MapRandSeed) & 0xF)) {
        return 2;
    }
    return cmp == ((11 * (avg / 4) + 61 * (x_diff / 4) + *MapRandSeed + 8) & 0x1F); // 0 or 1
}

/*
Purpose: Clear the map's site values in a radius from the tile.
Original Offset: 00592400
Return Value: n/a
Status: Complete
*/
void __cdecl site_radius(int x, int y, int UNUSED(unk_val)) {
    for (int i = 0; i < 21; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius)) {
            Map *tile = map_loc(x_radius, y_radius);
            tile->val2 &= 0x0F;
        }
    }
}

/*
Purpose: Search for the first landmark found within the radius range of the specified tile.
Original Offset: 00592550
Return Value: Landmark offset or -1 if none found
Status: Complete
*/
int __cdecl find_landmark(int x, int y, uint32_t radius_range_offset) {
    int radius = RadiusRange[radius_range_offset];
    for (int i = 0; i < radius; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius)) {
            for (int lm = 0; lm < *MapLandmarkCount; lm++) {
                if (x_radius == MapLandmark[lm].x && y_radius == MapLandmark[lm].y) {
                    return lm;
                }
            }
        }
    }
    return -1;
}

/*
Purpose: Set up a new landmark with the provided name at the specified tile.
Original Offset: 00592600
Return Value: Landmark offset or -1 if max landmark count is reached
Status: Complete
*/
int __cdecl new_landmark(int x, int y, LPCSTR name) {
    int landmark_offset = *MapLandmarkCount;
    if (landmark_offset >= MaxLandmarkNum) {
        return -1;
    }
    *MapLandmarkCount += 1;
    Landmark *lm = &MapLandmark[landmark_offset];
    lm->x = x;
    lm->y = y;
    strcpy_s(lm->name, 32, name);
    return landmark_offset;
}

/*
Purpose: Check whether the specified faction has permission to name a landmark on the provided tile.
Original Offset: 00592650
Return Value: Does the faction have control of the tile to set a landmark? true/false
Status: Complete
*/
BOOL __cdecl valid_landmark(uint32_t x, uint32_t y, int faction_id) {
    int terr_faction_id = *IsMultiplayerNet ? map_loc(x, y)->territory 
        : whose_territory(faction_id, x, y, NULL, false);
    if (terr_faction_id == faction_id) {
        return true;
    }
    if (terr_faction_id > 0) {
        return false;
    }
    int base_id = base_find(x, y);
    return base_id < 0 ? true : Bases[base_id].faction_id_current == faction_id;
}

/*
Purpose: Remove the landmark at the specified tile.
Original Offset: 005926F0
Return Value: n/a
Status: Complete
*/
void __cdecl kill_landmark(int x, int y) {
    int landmark_to_kill = find_landmark(x, y, 1);
    if (landmark_to_kill >= 0) {
        if (landmark_to_kill < (*MapLandmarkCount - 1)) {
            memcpy_s(&MapLandmark[landmark_to_kill], sizeof(Landmark) * MaxLandmarkNum,
                &MapLandmark[landmark_to_kill + 1], // single memcpy_s replaces original loop
                sizeof(Landmark) * (*MapLandmarkCount - landmark_to_kill - 1));
        }
        *MapLandmarkCount -= 1;
    }
}

/*
Purpose: Zero the five corner height entries a landmark tile code selects in one of the landmark
         tables.
Original Offset: n/a (inlined in delete_landmark() and fixup_landmarks())
Return Value: n/a
Status: Complete
*/
static void landmark_alt_clear(uint8_t *table, int code) {
    for (int i = 0; i < 5; i++) {
        table[LandmarkAltIndex[code * 5 + i]] = 0;
    }
}

/*
Purpose: Take the landmark relief off the specified tile: its entries in the landmark corner height
         tables are zeroed and its tile code is stored as -1 - code to mark the landmark as deleted,
         then every map view in use drops the cached heights around the tile. The bit 0x80
         (BIT2_FRESH) is cleared first even when the code is already negative. Ridges zero the
         mesa table, as in the original (the map view reads ridges from LandmarkAltRidge).
Original Offset: 005927D0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl delete_landmark(int x, int y) {
    Map *tile = map_loc(x, y);
    uint32_t bit2 = tile->bit2;
    if (!bit2) {
        return;
    }
    tile->bit2 = bit2 & ~BIT2_FRESH;
    int code = (int)tile->bit2 >> 24;
    if (code < 0) {
        return;
    }
    if (bit2 & BIT2_CRATER) {
        landmark_alt_clear(LandmarkAltCrater, code);
    }
    if (bit2 & BIT2_VOLCANO) {
        landmark_alt_clear(LandmarkAltVolcano, code);
    }
    if (bit2 & BIT2_CANYON) {
        landmark_alt_clear(LandmarkAltCanyon, code);
    }
    if (bit2 & BIT2_MESA) {
        landmark_alt_clear(LandmarkAltMesa, code);
    }
    if (bit2 & BIT2_RIDGE) {
        landmark_alt_clear(LandmarkAltMesa, code);
    }
    tile->bit2 = ((uint32_t)(-1 - code) << 24) | (tile->bit2 & 0xFFFFFF);
    *UnkBitfield1 |= 4;
    for (int i = 0; i < MaxMapWinNum; i++) {
        MapWin *win = MapWinList[i];
        if (win->draw_toggle_a) {
            win->unk_1 = 9999999;
            MapWin_clear_alt(win, 0, x, y, 1);
        }
    }
}

/*
Purpose: Put the landmark corner height tables back to their defaults, then zero the entries of the
         crater and volcano tiles whose landmark was deleted (negative tile code). Runs after a map
         is read or built.
Original Offset: 00592940
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl fixup_landmarks() {
    memcpy(LandmarkAltRidge, LandmarkAltRidgeDefault, LandmarkAltSize);
    memcpy(LandmarkAltMesa, LandmarkAltMesaDefault, LandmarkAltSize);
    memcpy(LandmarkAltCanyon, LandmarkAltCanyonDefault, LandmarkAltSize);
    memcpy(LandmarkAltVolcano, LandmarkAltVolcanoDefault, LandmarkAltSize);
    memcpy(LandmarkAltCrater, LandmarkAltCraterDefault, LandmarkAltSize);
    for (int y = 0; y < *MapLatitudeBounds; y++) {
        for (int x = y & 1; x < *MapLongitudeBounds; x += 2) {
            uint32_t bit2 = map_loc(x, y)->bit2;
            int code = (int)bit2 >> 24;
            if (bit2 && code < 0) {
                code = -1 - code;
                if (bit2 & BIT2_VOLCANO) {
                    landmark_alt_clear(LandmarkAltVolcano, code);
                }
                if (bit2 & BIT2_CRATER) {
                    landmark_alt_clear(LandmarkAltCrater, code);
                }
            }
        }
    }
}

/*
Purpose: Give a map view a fresh drawing cache for the whole map: 12 bytes per tile, the eight
         height bytes (0 to 8) set to 0xFF to be worked out again and byte 9 holding the bits 0xD0
         of the first map tile's climate byte, for every entry (as in the original). The old
         cache is freed. Bytes 10 and 11 are left as allocated.
Original Offset: 004711A0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl terrain_fixup(MapWin *win) {
    if (!win) {
        return;
    }
    if (win->tile_cache) {
        _free(win->tile_cache);
        win->tile_cache = 0;
    }
    win->tile_cache = (uint8_t *)mem_get_old(*MapArea * 12);
    for (int i = 0; i < (int)*MapArea; i++) {
        uint8_t *entry = &win->tile_cache[i * 12];
        for (int j = 0; j < 4; j++) {
            entry[j] = 0xFF;
        }
        for (int j = 4; j < 9; j++) {
            entry[j] = 0xFF;
        }
        entry[9] = 0;
        entry[9] |= (*MapTiles)->climate & 0xD0;
    }
}

/*
Purpose: Rebuild the drawing caches of the map views after a new map is set up: the main map, the
         other listed views in use, and the three views outside the list.
Original Offset: 00471240
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-17-council-batch28.log)
*/
void __cdecl mapwin_terrain_fixup() {
    for (int i = 0; i < MaxMapWinNum; i++) {
        if (!i || MapWinList[i]->draw_toggle_a) {
            terrain_fixup(MapWinList[i]);
        }
    }
    terrain_fixup(MapWinUnk);
    terrain_fixup(MapWinUnk2);
    terrain_fixup(BaseMapWin);
}

/*
Purpose: Check if coordinates are considered near or on coast. Radius (excludes actual coordinates)
         can either be all the squares directly around the coordinates or same as Base '+' radius.
Original Offset: 004E49D0
Return Value: Is tile coast? true/false
Status: Complete
*/
BOOL __cdecl is_coast(int x, int y, BOOL is_base_radius) {
    uint32_t radius = is_base_radius ? 21 : 9;
    for (uint32_t i = 1; i < radius; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius) && is_ocean(x_radius, y_radius)) {
            return true; // modified original that would return i, all calls check return as boolean
        }
    }
    return false;
}

/*
Purpose: Check whether the specified tile is part of an ocean.
Original Offset: 005001E0
Return Value: Is tile ocean? true/false
Status: Complete
*/
BOOL __cdecl is_ocean(uint32_t x, uint32_t y) {
    return altitude_at(x, y) < ALT_BIT_SHORE_LINE;
}

/*
Purpose: Get the owner of the specified tile if there is a unit in it.
Original Offset: 00500250
Return Value: Owner (faction id) or -1
Status: Complete
*/
int __cdecl veh_who(uint32_t x, uint32_t y) {
    Map *tile = map_loc(x, y);
    if (tile->bit & BIT_VEH_IN_TILE) {
        uint32_t owner = tile->val2 & 0xF;
        if (owner < 8) {
            return owner;
        }
    }
    return -1;
}

/*
Purpose: Recount every region's territory statistics from scratch. Each tile goes to the faction
         whose base is nearest within the territory distance (half of it at sea, where a base on or
         next to the tile also claims it); a change of owner flags the tile for redraw. Per region
         and owner this tallies territory, visible tiles, good land (moist, not rocky, no fungus)
         and the part of it inside base radii, the best site rating and supply pods. Pods in a
         faction's territory, or unclaimed pods near a faction's bases, become AI scouting sites.
Original Offset: 00523DD0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch12.log)
*/
void __cdecl reset_territory() {
    for (int faction_id = 0; faction_id < MaxPlayerNum; faction_id++) {
        for (int region = 0; region < MaxContinentNum; region++) {
            PlayersData[faction_id].region_territory_tiles[region] = 0;
            PlayersData[faction_id].region_visible_tiles[region] = 0;
            PlayersData[faction_id].region_good_tiles[region] = 0;
            PlayersData[faction_id].region_good_radius_tiles[region] = 0;
            PlayersData[faction_id].region_best_site[region] = 0;
            PlayersData[faction_id].region_territory_pods[region] = 0;
        }
    }
    for (int region = 0; region < MaxContinentNum; region++) {
        Continents[region].unk_3 = 0;
        Continents[region].pods = 0;
    }
    for (int y = 0; y < (int)*MapLatitudeBounds; y++) {
        for (int x = y & 1; x < (int)*MapLongitudeBounds; x += 2) {
            Map *tile = map_loc(x, y);
            uint32_t region = tile->region;
            for (uint32_t faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
                uint32_t flags = PlayersData[faction_id].flags;
                if ((flags & PFLAG_STRAT_SEARCH_OBJECTIVES)
                    ? (tile->bit & BIT_SUPPLY_REMOVE) != 0
                    : ((flags & PFLAG_MAP_REVEALED) || (tile->visibility & (1 << faction_id)))) {
                    PlayersData[faction_id].region_visible_tiles[region]++;
                }
            }
            int owner = -1;
            int base_id = base_find(x, y, -1, region, -1, -1);
            BOOL ocean = (tile->climate & 0xE0) < ALT_BIT_SHORE_LINE;
            if (base_id >= 0) {
                int dist = Rules->territory_max_dist_base;
                if (ocean) {
                    dist >>= 1;
                }
                if (*BaseFindDist < dist) {
                    owner = Bases[base_id].faction_id_current;
                }
            }
            if (ocean) {
                for (int i = 0; i < RadiusRange[1]; i++) {
                    int x_radius = x + RadiusOffsetX[i];
                    if (!*MapIsFlat) {
                        if (x_radius < 0) {
                            x_radius += *MapLongitudeBounds;
                        } else if (x_radius >= (int)*MapLongitudeBounds) {
                            x_radius -= *MapLongitudeBounds;
                        }
                    }
                    int y_radius = y + RadiusOffsetY[i];
                    if (y_radius >= 0 && y_radius < (int)*MapLatitudeBounds && x_radius >= 0
                        && x_radius < (int)*MapLongitudeBounds) {
                        int base_id_adj = base_at(x_radius, y_radius);
                        if (base_id_adj >= 0) {
                            owner = Bases[base_id_adj].faction_id_current;
                            break;
                        }
                    }
                }
            }
            if (owner != tile->territory) {
                tile->bit2 |= BIT2_UNK_400000;
                *UnkBitfield1 |= 1;
            }
            tile->territory = (int8_t)owner;
            if (owner < 0) {
                owner = 0;
            }
            PlayerData *player = &PlayersData[owner];
            player->region_territory_tiles[region]++;
            if ((tile->climate & (RAINFALL_RAINY | RAINFALL_MOIST)) && (tile->val3 & 0xC0) < 0x80
                && !(tile->bit & BIT_FUNGUS)) {
                player->region_good_tiles[region]++;
                if (tile->bit & BIT_BASE_RADIUS) {
                    player->region_good_radius_tiles[region]++;
                }
            }
            int site = site_at(x, y, 0);
            if ((int)Continents[region].unk_3 < site) {
                Continents[region].unk_3 = site;
            }
            if (owner && player->region_best_site[region] < site) {
                player->region_best_site[region] = (uint8_t)site;
            }
            if (!goody_at(x, y)) {
                continue;
            }
            Continents[region].pods++;
            player->region_territory_pods[region]++;
            if (owner) {
                if ((tile->visibility & (1 << owner))
                    || (!is_human(owner) && *DiffLevelCurrent > DLVL_LIBRARIAN)) {
                    add_site(owner, AI_GOAL_SCOUT, 1, x, y);
                }
                continue;
            }
            for (uint32_t faction_id = 1; faction_id < MaxPlayerNum; faction_id++) {
                if (PlayersData[faction_id].region_total_bases[region]
                    && ((tile->visibility & (1 << faction_id))
                        || (!is_human(faction_id) && *DiffLevelCurrent > DLVL_LIBRARIAN))
                    && !at_site(faction_id, AI_GOAL_SCOUT, x, y)
                    && base_find(x, y, faction_id, region, -1, -1) >= 0
                    && (int)Rules->territory_max_dist_base <= *BaseFindDist
                    && *BaseFindDist <= (int)Rules->territory_max_dist_base * 2) {
                    add_site(faction_id, AI_GOAL_SCOUT, 1, x, y);
                }
            }
        }
    }
}

/*
Purpose: Rebuild the Map's unit related values.
Original Offset: 00532A90
Return Value: n/a
Status: Complete
*/
void __cdecl rebuild_vehicle_bits() {
    for (int y = 0; y < *MapLatitudeBounds; y++) {
        for (int x = y & 1; x < *MapLongitudeBounds; x += 2) {
            bit_set(x, y, BIT_VEH_IN_TILE, false);
            for (int veh_id = 0; veh_id < *VehCurrentCount; veh_id++) {
                if (Vehs[veh_id].x == (int)x && Vehs[veh_id].y == (int)y) {
                    bit_set(x, y, BIT_VEH_IN_TILE, true);
                    if (!(bit_at(x, y) & BIT_BASE_IN_TILE)) {
                        owner_set(x, y, Vehs[veh_id].faction_id);
                    }
                    break;
                }
            }
        }
    }
}

/*
Purpose: Rebuild the Map's base related values.
Original Offset: 00532B70
Return Value: n/a
Status: Complete
*/
void __cdecl rebuild_base_bits() {
    for (int y = 0; y < *MapLatitudeBounds; y++) {
        for (int x = y & 1; x < *MapLongitudeBounds; x += 2) {
            bit_set(x, y, BIT_BASE_IN_TILE, false);
            for (int base_id = 0; base_id < *BaseCurrentCount; base_id++) {
                if (Bases[base_id].x == (int)x && Bases[base_id].y == (int)y) {
                    bit_set(x, y, BIT_BASE_IN_TILE, true);
                    owner_set(x, y, Bases[base_id].faction_id_current);
                    break;
                }
            }
        }
    }
}

/*
Purpose: Calculate the distance between two x coordinates with handling for round maps.
Original Offset: 00579790
Return Value: Distance
Status: Complete
*/
int __cdecl x_dist(int x_point_a, int x_point_b) {
    int dist = abs(x_point_a - x_point_b);
    if (!*MapIsFlat && dist > (int)*MapLongitude) {
        dist = *MapLongitudeBounds - dist;
    }
    return dist;
}

/*
Purpose: Get the base site rating of a tile, computing and storing it with world_site() the first
         time. Tiles inside an existing base radius rate zero.
Original Offset: 005797D0
Return Value: Site rating (0-15)
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch3.log)
*/
uint32_t __cdecl site_at(int x, int y, int unk) {
    Map *tile = map_loc(x, y);
    if (tile->bit & BIT_BASE_RADIUS) {
        return 0;
    }
    uint32_t site = tile->val2 >> 4;
    if (!site) {
        site = world_site(x, y, unk);
        site_set(x, y, site);
    }
    return site;
}

/*
Purpose: Check whether a faction can see the specified tile.
Original Offset: 00579840
Return Value: Is tile visible/known to faction? true/false
Status: Complete
*/
BOOL __cdecl is_known(uint32_t x, uint32_t y, uint32_t faction_id) {
    return (PlayersData[faction_id].flags & PFLAG_MAP_REVEALED
        || map_loc(x, y)->visibility & (1 << faction_id));
}

/*
Purpose: If a base exists, get the owner of the specified tile.
Original Offset: 005798A0
Return Value: Owner (faction id) or -1
Status: Complete
*/
int __cdecl base_who(uint32_t x, uint32_t y) {
    Map *tile = map_loc(x, y);
    if (tile->bit & BIT_BASE_IN_TILE) {
        uint32_t owner = tile->val2 & 0xF;
        if (owner < MaxPlayerNum) {
            return owner;
        }
    }
    return -1;
}

/*
Purpose: If a base or unit exists, get the owner of the specified tile.
Original Offset: 005798E0
Return Value: Owner (faction id) or -1
Status: Complete
*/
int __cdecl anything_at(uint32_t x, uint32_t y) {
    Map *tile = map_loc(x, y);
    if (tile->bit & (BIT_VEH_IN_TILE | BIT_BASE_IN_TILE)) {
        uint32_t owner = tile->val2 & 0xF;
        if (owner < MaxPlayerNum) {
            return owner;
        }
    }
    return -1;
}

/*
Purpose: Shutdown allocated map variables.
Original Offset: 00590E90
Return Value: n/a
Status: Complete
*/
void __cdecl map_shutdown() {
    if (*MapTiles) {
        free(*MapTiles);
    }
    if (*MapAbstract) {
        free(*MapAbstract);
    }
    *MapTiles = 0;
    *MapAbstract = 0;
}

/*
Purpose: Initialize map variables.
Original Offset: 00590ED0
Return Value: n/a
Status: Complete
*/
BOOL __cdecl map_init() {
    sprintf_s((LPSTR)MapFilePath, 80, "maps\\%s.%s", label_get(676), MapExtension);
    *MapLongitude = *MapLongitudeBounds / 2;
    *MapArea = *MapLongitude * *MapLatitudeBounds;
    *MapAreaSqRoot = quick_root(*MapArea);
    *MapTiles = 0;
    *MapTiles = (Map *)mem_get(*MapArea * sizeof(Map));
    if (*MapTiles) {
        *MapAbstractLongBounds = (*MapLongitudeBounds + 4) / 5;
        *MapAbstractLatBounds = (*MapLatitudeBounds + 4) / 5;
        *MapAbstractArea = *MapAbstractLatBounds * ((*MapAbstractLongBounds + 1) / 2);
        *MapAbstract = (uint8_t *)mem_get(*MapAbstractArea);
        if (*MapAbstract) {
            mapwin_terrain_fixup();
            return false;
        }
    }
    map_shutdown();
    return true;
}

/*
Purpose: Reset the map to a blank state. Doesn't wipe unk_1 and territory fields.
Original Offset: 00591040
Return Value: n/a
Status: Complete
*/
void __cdecl map_wipe() {
    *MapSeaLevel = 0;
    *MapSeaLevelCouncil = 0;
    *MapLandmarkCount = 0;
    *MapRandSeed = random(0, 0x7FFF) + 1;
    for (uint32_t i = 0; i < *MapArea; i++) {
        (*MapTiles)[i].climate = ALT_BIT_OCEAN;
        (*MapTiles)[i].contour = 20;
        (*MapTiles)[i].val2 = 0xF;
        (*MapTiles)[i].region = 0;
        (*MapTiles)[i].visibility = 0;
        (*MapTiles)[i].val3 = 0;
        (*MapTiles)[i].bit = 0;
        (*MapTiles)[i].bit2 = 0;
        ZeroMemory((*MapTiles)[i].bit_visible, sizeof((*MapTiles)[i].bit_visible));
    }
}

/*
Purpose: Write map data to a file.
Original Offset: 005910B0
Return Value: Did an error occur? true/false
Status: Complete
*/
BOOL __cdecl map_write(FILE *map_file) {
    if (_fwrite(&*MapLongitudeBounds, 2724, 1, map_file)
        && _fwrite(*MapTiles, *MapArea * sizeof(Map), 1, map_file)
        && _fwrite(*MapAbstract, *MapAbstractArea, 1, map_file)) {
        return false;
    }
    return true;
}

/*
Purpose: Read the map data from a file and write it into memory.
Original Offset: 00591130
Return Value: Did an error occur? true/false
Status: Complete
*/
BOOL __cdecl map_read(FILE *map_file) {
    map_shutdown();
    if (!_fread(&*MapLongitudeBounds, 2724, 1, map_file)) {
        return true;
    }
    *MapTiles = 0;
    *MapAbstract = 0;
    if (map_init()) {
        return true;
    }
    if (!_fread(*MapTiles, *MapArea * sizeof(Map), 1, map_file)
        || !_fread(*MapAbstract, *MapAbstractArea, 1, map_file)) {
        return true;
    }
    fixup_landmarks();
    return false;
}

/*
Purpose: Get the region value for the specified tile.
Original Offset: 00591210
Return Value: Abstract value (region)
Status: Complete
*/
uint8_t __cdecl abstract_at(uint32_t x, uint32_t y) {
    return (*MapAbstract)[(x >> 1) + y * (*MapAbstractLongBounds >> 1)];
}

/*
Purpose: Set the region value for the specified tile.
Original Offset: 00591230
Return Value: n/a
Status: Complete
*/
void __cdecl abstract_set(uint32_t x, uint32_t y, uint8_t region) {
    (*MapAbstract)[(x >> 1) + y * (*MapAbstractLongBounds >> 1)] = region;
}

/*
Purpose: Flag every map tile for redraw and note that the whole map needs refreshing.
Original Offset: 00592A80
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch19.log)
*/
void __cdecl set_dirty() {
    for (int y = 0; y < (int)*MapLatitudeBounds; y++) {
        for (int x = y & 1; x < (int)*MapLongitudeBounds; x += 2) {
            map_loc(x, y)->bit2 |= BIT2_UNK_400000;
        }
    }
    *UnkBitfield1 |= 4;
}

/*
Purpose: Flag every map tile for redraw. Despite the name it marks the tiles the same way
         set_dirty() does, without the whole-map refresh flag.
Original Offset: 00592AF0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch19.log)
*/
void __cdecl clear_dirty() {
    for (int y = 0; y < (int)*MapLatitudeBounds; y++) {
        for (int x = y & 1; x < (int)*MapLongitudeBounds; x += 2) {
            map_loc(x, y)->bit2 |= BIT2_UNK_400000;
        }
    }
}

/*
Purpose: Quickly check for unit related zone of control conflicts. If a ZOC conflict is found, store
         the coordinates of the tile inside ZOC pointers.
Original Offset: 00593830
Return Value: n/a
Status: Complete
*/
void __cdecl quick_zoc(uint32_t x_src, uint32_t y_src, uint32_t faction_id, int x_dst, int y_dst, 
                       int *x_zoc, int *y_zoc) {
    BOOL is_src_ocean = is_ocean(x_src, y_src);
    int search_zoc = -1;
    for (uint32_t i = 0; i < 8; i++) {
        int x_radius = xrange(x_src + RadiusBaseX[i]);
        int y_radius = y_src + RadiusBaseY[i];
        if (on_map(x_radius, y_radius)) {
            int owner = veh_who(x_radius, y_radius);
            if (owner >= 0 && (uint32_t)owner != faction_id 
                && is_ocean(x_radius, y_radius) == is_src_ocean
                && !has_treaty(faction_id, owner, DTREATY_PACT)) {
                int proximity = vector_dist(x_radius, y_radius, x_dst, y_dst);
                if (proximity >= search_zoc) {
                    search_zoc = proximity;
                    *x_zoc = x_radius;
                    *y_zoc = y_radius;
                }
            }
        }
    }
}

/*
Purpose: Determine if the specified offsets are within the range radius.
Original Offset: 005A65A0
Return Value: Range radius, otherwise -1 if not within range
Status: Complete
*/
int __cdecl radius_move(int x_radius_off, int y_radius_off, int range) {
    for (int i = 0; i < range; i++) {
        if (x_radius_off == RadiusOffsetX[i] && y_radius_off == RadiusOffsetY[i]) {
            return i; // radius
        }
    }
    return -1;
}

/*
Purpose: Determine if the specified two tiles are within the range radius of each other.
Original Offset: 005A65D0
Return Value: Range radius, otherwise -1 if not within range
Status: Complete
*/
int __cdecl radius_move(int x_src, int y_src, int x_dst, int y_dst, int range) {
    int x_radius_off = x_dst - x_src;
    if (x_radius_off < (-(int)*MapLongitude)) {
        x_radius_off += *MapLongitudeBounds;
    }
    if (x_radius_off > ((int)*MapLongitude)) {
        x_radius_off -= *MapLongitudeBounds;
    }
    return radius_move(x_radius_off, y_dst - y_src, range);
}

/*
Purpose: Determine if the specified two tiles are within the radius directionally of each other.
Original Offset: 005A6630
Return Value: Range radius, otherwise -1 if not within range
Status: Complete
*/
int __cdecl compass_move(int x_src, int y_src, int x_dst, int y_dst) {
    int x_radius_off = x_dst - x_src;
    if (x_radius_off < (-(int)*MapLongitude)) {
        x_radius_off += *MapLongitudeBounds;
    }
    if (x_radius_off > ((int)*MapLongitude)) {
        x_radius_off -= *MapLongitudeBounds;
    }
    int y_radius_off = y_dst - y_src;
    int direction_x = (x_radius_off > 0) ? 1 : (x_radius_off >= 0) - 1;
    int direction_y = (y_radius_off > 0) ? 1 : (y_radius_off >= 0) - 1;
    for (int i = 0; i < 9; i++) {
        if (direction_x == (RadiusBaseX[i] > 0 ? 1 : (RadiusBaseX[i] >= 0) - 1)
            && direction_y == (RadiusBaseY[i] > 0 ? 1 : (RadiusBaseY[i] >= 0) - 1)) {
            return i;
        }
    }
    return -1;
}

/*
Purpose: Check whether there is a sensor available in the specified tile.
Original Offset: 005BF010
Return Value: 0 (no sensor), 1 (sensor array via terraforming), 2 (Geosynchronous Survey Pod)
Status: Complete
*/
int __cdecl is_sensor(uint32_t x, uint32_t y) {
    if (bit_at(x, y) & BIT_SENSOR_ARRAY) {
        return 1; // Sensor Array built in tile
    }
    int base_id = base_find(x, y);
    if (base_id != -1) {
        int dist_x = x_dist(x, Bases[base_id].x);
        if (!dist_x || dist_x == 2) { // removed unnecessary duplicate calculation of distX
            int dist_y = abs((int)y - Bases[base_id].y);
            if (!dist_y || dist_y == 2) {
                if (has_fac_built(FAC_GEOSYNC_SURVEY_POD, base_id)) {
                    return 2; // Geosynchronous Survey Pod
                }
            }
        }
    }
    return 0; // No sensor found
}

/*
Purpose: Check if faction controls the initial tile (code offset 0) of the Manifold Nexus.
Original Offset: 005BF130
Return Value: Does faction control Nexus? true/false
Status: Complete
*/
BOOL __cdecl has_temple(int faction_id) {
    for (int y = 0; y < *MapLatitudeBounds; y++) {
        for (int x = y & 1; x < *MapLongitudeBounds; x += 2) {
            if ((bit2_at(x, y) & (BIT2_UNK_80000000 | BIT2_NEXUS)) == BIT2_NEXUS
                && !code_at(x, y) 
                && faction_id == whose_territory(faction_id, x, y, NULL, false)
                && map_loc(x, y)->visibility & (1 << faction_id)) { // tile visible
                return true;
            }
        }
    }
    return false;
}

/*
Purpose: Handle setting the world altitude.
Original Offset: 005C2020
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_alt_set(int x, int y, int altitude, BOOL is_set_both) {
    ZeroMemory(MapBaseSubmergedCount, sizeof(int) * MaxPlayerNum);
    memset(MapBaseIdClosestSubmergedVeh, -1, sizeof(int) * MaxPlayerNum);
    if (on_map(x, y)) {
        is_set_both ? alt_set_both(x, y, altitude) : alt_set(x, y, altitude);
    }
    BOOL has_set_alt;
    for (int i = 1, alt = altitude - 1; i < altitude; i++, alt--) {
        has_set_alt = false;
        for (int j = RadiusRange[i - 1]; j < RadiusRange[i]; j++) { // ring i
            int x_radius = xrange(x + RadiusOffsetX[j]);
            int y_radius = y + RadiusOffsetY[j];
            if (on_map(x_radius, y_radius) && alt_at(x_radius, y_radius) < alt) {
                if (is_set_both) {
                    alt_set_both(x_radius, y_radius, alt);
                    if (anything_at(x_radius, y_radius) < 0 && !bonus_at(x_radius, y_radius, 0)) {
                        owner_set(x_radius, y_radius, -1);
                    }
                } else {
                    alt_set(x_radius, y_radius, alt);
                }
                has_set_alt = true;
            }
        }
        if (!has_set_alt) {
            break;
        }
    }
    int max_alt = 8 - altitude;
    for (int i = 1, alt = i + altitude; i < max_alt; i++, alt++) {
        has_set_alt = false;
        for (int j = RadiusRange[i - 1]; j < RadiusRange[i]; j++) { // ring i
            int x_radius = xrange(x + RadiusOffsetX[j]);
            int y_radius = y + RadiusOffsetY[j];
            // outward rings are pulled DOWN toward the centre altitude (original: alt < alt_at)
            if (on_map(x_radius, y_radius) && alt_at(x_radius, y_radius) > alt) {
                if (is_set_both) {
                    alt_set_both(x_radius, y_radius, alt);
                    if (anything_at(x_radius, y_radius) < 0 && !bonus_at(x_radius, y_radius, 0)) {
                        owner_set(x_radius, y_radius, -1);
                    }
                } else {
                    alt_set(x_radius, y_radius, alt);
                }
                has_set_alt = true;
            }
        }
        if (!has_set_alt) {
            break;
        }
    }
}

/*
Purpose: Handle raising the altitude of the specified tile.
Original Offset: 005C2380
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_raise_alt(int x, int y) {
    if (on_map(x, y)) {
        uint32_t altitude = alt_at(x, y);
        if (altitude < 6) {
            world_alt_set(x, y, altitude + 1, true);
        }
    }
}

/*
Purpose: Handle lowering the altitude of the specified tile.
Original Offset: 005C23E0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_lower_alt(int x, int y) {
    if (on_map(x, y)) {
        uint32_t altitude = alt_at(x, y);
        if (altitude) {
            world_alt_set(x, y, altitude - 1, true);
        }
    }
}

/*
Purpose: Set up the brush for creating world terrain.
Original Offset: 005C2440
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl brush(int x, int y, int altitude) {
    BOOL use_draw_radius = false;
    for (int i = 0; i < 4; i++) {
        if (i != 2) {
            int x_radius = xrange(x + RadiusOffsetX[i]);
            int y_radius = y + RadiusOffsetY[i];
            if (on_map(x_radius, y_radius) && alt_at(x_radius, y_radius) != altitude) {
                BOOL skip = false;
                for (uint32_t j = 0; j < 8; j++) {
                    int x_rad_base = xrange(x_radius + RadiusBaseX[j]);
                    int y_rad_base = y_radius + RadiusBaseY[j];
                    if (on_map(x_rad_base, y_rad_base) && !is_ocean(x_rad_base, y_rad_base)
                        && !region_at(x_rad_base, y_rad_base)) {
                        skip = true;
                    }
                }
                if (!skip) {
                    if (altitude == ALT_SHORE_LINE) {
                        for (uint32_t k = 0; k < 8; k += 2) {
                            int x_rad_base = xrange(x_radius + RadiusBaseX[k + 1]);
                            int y_rad_base = y_radius + RadiusBaseY[k + 1];
                            if (on_map(x_rad_base, y_rad_base)
                                && altitude_at(x_rad_base, y_rad_base) == ALT_BIT_SHORE_LINE) {
                                int x_rad_base2 = xrange(x_radius + RadiusBaseX[(k + 2) % 8]);
                                int y_rad_base2 = y_radius + RadiusBaseY[(k + 2) % 8];
                                int x_rad_base3 = xrange(x_radius + RadiusBaseX[k % 8]);
                                int y_rad_base3 = y_radius + RadiusBaseY[k % 8];
                                if (on_map(x_rad_base2, y_rad_base2) 
                                    && on_map(x_rad_base3, y_rad_base3)
                                    && altitude_at(x_rad_base2, y_rad_base2) < ALT_BIT_SHORE_LINE
                                    && altitude_at(x_rad_base3, y_rad_base3) < ALT_BIT_SHORE_LINE) {
                                    skip = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (!skip) {
                        region_set(x_radius, y_radius, 1);
                        use_draw_radius |= alt_at(x_radius, y_radius) != altitude;
                        world_alt_set(x_radius, y_radius, altitude, true);
                        if (*GameState & STATE_OMNISCIENT_VIEW && altitude > 3) {
                            temp_set(x_radius, y_radius, 1);
                        }
                        *BrushVal1 += 1;
                        *BrushVal2 += 1;
                    }
                }
            }
        }
    }
    if (*GameState & STATE_OMNISCIENT_VIEW && use_draw_radius) {
        draw_radius(x, y, 2, 2);
    }
}

/*
Purpose: Paint land to assist in the creation of the world terrain.
Original Offset: 005C27F0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl paint_land(int x, int y, uint32_t altitude, uint32_t radius) {
    *BrushVal2 = 0;
    int i = 0;
    do {
        int search = -1;
        uint32_t unk_val = 0;
        int x_rad_base = x;
        int y_rad_base = y;
        do {
            brush(x_rad_base, y_rad_base, altitude);
            uint32_t offset = _rand() % 8 | 1;
            if ((int)offset == search) {
                if (++unk_val > ((*MapLandCoverage * *MapLandCoverage) + 2)) {
                    offset = (offset - (*MapLandCoverage * *MapLandCoverage) + unk_val - 2) % 8;
                }
            } else {
                search = offset;
                unk_val = 0;
            }
            x_rad_base = xrange(x_rad_base + RadiusBaseX[offset]); // walk on from the current tile
            y_rad_base += RadiusBaseY[offset];
            i++;
        } while (i < 2000 && on_map(x_rad_base, y_rad_base) && *BrushVal2 < radius);
        i++;
    } while (i < 2000 && *BrushVal2 < radius);
}

/*
Purpose: Build out the map continents.
Original Offset: 005C28F0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl build_continent(uint32_t size) {
    for (uint32_t i = 0; i < *MapArea; i++) {
        (*MapTiles)[i].region = 0;
    }
    int coverage = *MapLandCoverage;
    if (coverage && *BrushVal1 >= *WorldBuildVal1) {
        coverage--;
    }
    int radius = WorldBuilder->continent_mod * coverage * coverage + WorldBuilder->continent_base;
    int x; 
    int y;
    int i = 0;
    do {
        x = rnd(*MapLongitudeBounds - (*MapIsFlat * 8), NULL) + *MapIsFlat * 4;
        y = rnd(*MapLatitudeBounds - 8, NULL) + 4;
        if (x & 1) {
            x--;
        }
        if (y & 1) {
            x++; // validate whether this should be y and/or reasoning for x
        }
    } while (++i < 100 && !is_ocean(x, y));
    if (is_ocean(x, y)) {
        uint32_t ratio = (size * 3200) / *MapArea;
        if (ratio > WorldBuilder->cont_size_ratio5 || *BrushVal1 >= *WorldBuildVal1) {
            radius /= 4;
            if (*BrushVal1 >= *WorldBuildVal1 && ratio > WorldBuilder->cont_size_ratio5) {
                radius = _rand() % 3 + 1;
            }
        } else if (ratio <= WorldBuilder->cont_size_ratio4) {
            if (ratio <= WorldBuilder->cont_size_ratio3) {
                if (ratio <= WorldBuilder->cont_size_ratio2) {
                    if (ratio < WorldBuilder->cont_size_ratio1 && *MapLandCoverage > 1) {
                        radius += radius / 2;
                    }
                } else  if (rnd(4 - *MapLandCoverage, NULL)) {
                    radius /= 2;
                }
            } else {
                radius /= 2;
            }
        } else {
            radius /= 3;
        }
        if (*BrushVal1 < *WorldBuildVal1 && radius < 4) {
            radius = 4;
        }
        paint_land(x, y, ALT_SHORE_LINE, radius);
        do_all_non_input();
    }
}

/*
Purpose: Build out the map hills.
Original Offset: 005C2B40
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl build_hills(int altitude) {
    int i = 0;
    while (true) {
        int x = rnd(*MapLongitudeBounds - (*MapIsFlat * 8), NULL) + *MapIsFlat * 4;
        int y = rnd(*MapLatitudeBounds - 8, NULL) + 4;
        if (x & 1) {
            x--;
        }
        if (y & 1) {
            x++; // should this have been y?
        }
        i += 2;
        if (i > 100) {
            return;
        }
        BOOL is_too_low = false;
        for (int j = 0; j < RadiusRange[1]; j++) {
            int x_radius = xrange(x + RadiusOffsetX[j]);
            int y_radius = y + RadiusOffsetY[j];
            if (on_map(x_radius, y_radius) && alt_at(x_radius, y_radius) < (altitude - 1)) {
                is_too_low = true;
                break;
            }
        }
        if (is_too_low) {
            continue; // draw another location
        }
        int plat_mod = WorldBuilder->plateau_mod * (2 - *MapOceanCoverage);
        if (altitude != ALT_1_LEVEL_ABOVE_SEA) {
            plat_mod /= 4;
        }
        uint32_t plat_base = WorldBuilder->plateau_base;
        if (*MapOceanCoverage >= 2) {
            plat_base /= 2;
        }
        paint_land(x, y, altitude, plat_mod + plat_base);
        do_all_non_input();
        return;
    }
}

/*
Purpose: Random lattice used by world_erosion() and world_fungus(): a 16x16 grid of values 0-15,
         wrapped on both axes, sampled with bilinear interpolation. The original is a stack
         object with two methods; its smoothing pass sums each cell's neighbours and then
         discards the sum, so only the copy survives.
Original Offset: 005C1E20 (init), 005C1F20 (at)
Return Value: n/a / interpolated noise (0-480)
Status: Complete (verified vs original through world_fungus())
*/
struct WorldNoise {
    int8_t value[17][19];
    uint8_t unused[0x26];
    int8_t raw[19][19];

    void init() {
        for (int row = 0; row < 18; row++) {
            for (int col = 0; col < 18; col++) {
                raw[row][col] = (int8_t)(_rand() % 16); // the exe's CRT generator
            }
            raw[row][18] = raw[row][0];
        }
        for (int col = 0; col < 18; col++) {
            raw[18][col] = raw[0][col];
        }
        for (int row = 0; row < 17; row++) {
            for (int col = 0; col < 17; col++) {
                value[row][col] = raw[row][col];
            }
        }
        for (int row = 0; row < 17; row++) {
            value[row][16] = value[row][0];
        }
        for (int col = 0; col < 17; col++) {
            value[16][col] = value[0][col];
        }
    }

    int at(int x, int y) {
        uint32_t cell_x = ((x - 0x80) >> 8) & 0xF;
        uint32_t cell_y = ((y - 0x80) >> 8) & 0xF;
        int frac_x = ((x - 0x80) >> 3) & 0x1F;
        int frac_y = ((y - 0x80) >> 3) & 0x1F;
        const int8_t *p = &value[cell_x][cell_y];
        return ((p[19] * frac_x + p[0] * (32 - frac_x)) * (32 - frac_y)
            + (p[1] * (32 - frac_x) + p[20] * frac_x) * frac_y) >> 5;
    }
};

/*
Purpose: Erode the contours in two passes over the map (sea and shelf tiles first, then the land,
         after the shorelines are redone), mixing each tile's contour with a value from the
         random lattice and the world builder rules: deep sea sinks, the shelf rises when crowded
         by water and is pushed up near a big landmass or between two, low land (altitude 3) is
         raised by the plains rule and, below its contour, by the hills rule (more when few
         neighbours are water and when another landmass is two tiles away), and higher land by the
         hills and peaks rules. Without erosive forces the result is capped at 74. The plains and
         beach terms scale with the map size when the land coverage is 1. The continents are
         recounted before each pass.
Original Offset: 005C2CB0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch29.log)
*/
void __cdecl world_erosion() {
    WorldNoise noise;
    noise.init();
    int root_base, root_area;
    if (*MapLandCoverage == 1) {
        root_base = quick_root(3200);
        root_area = quick_root(*MapArea);
    } else {
        root_area = 1;
        root_base = 1;
    }
    const int erosive = (int)*MapErosiveForces;
    for (int pass = 0; pass < 2; pass++) {
        if (pass) {
            world_shorelines();
        }
        Paths->continents();
        for (int y = 0; y < *MapLatitudeBounds; y++) {
            for (int x = y & 1; x < *MapLongitudeBounds; x += 2) {
                Map *tile = map_loc(x, y);
                int alt = tile->climate >> 5;
                int contour = tile->contour;
                int value = (noise.at(x << 4, y << 4) * 5 + noise.at(x << 7, y << 7) * 2) * 7 >> 8;
                value = value < 0 ? 0 : (value > 100 ? 100 : value);
                if (pass ? alt <= 2 : alt > 2) {
                    continue;
                }
                int water = 0; // neighbours at altitude 2 or below
                int landmass = 0; // a large landmass next to the shelf, or land of another region
                if (alt == 2 || alt == 3) {
                    BOOL is_flat = *MapIsFlat & 1;
                    for (int i = 0; i < 8; i++) {
                        int x_near = x + RadiusBaseX[i];
                        if (!is_flat) {
                            if (x_near < 0) {
                                x_near += *MapLongitudeBounds;
                            } else if (x_near >= *MapLongitudeBounds) {
                                x_near -= *MapLongitudeBounds;
                            }
                        }
                        int y_near = y + RadiusBaseY[i];
                        if (y_near >= 0 && y_near < *MapLatitudeBounds && x_near >= 0
                            && x_near < *MapLongitudeBounds
                            && (map_loc(x_near, y_near)->climate & 0xE0) <= 0x40) {
                            water++;
                        }
                    }
                    if (alt == 3) {
                        uint32_t region = tile->region;
                        for (int i = 9; i < 25; i++) { // the ring two tiles out
                            int x_near = x + RadiusOffsetX[i];
                            if (!is_flat) {
                                if (x_near < 0) {
                                    x_near += *MapLongitudeBounds;
                                } else if (x_near >= *MapLongitudeBounds) {
                                    x_near -= *MapLongitudeBounds;
                                }
                            }
                            int y_near = y + RadiusOffsetY[i];
                            if (y_near < 0 || y_near >= *MapLatitudeBounds || x_near < 0
                                || x_near >= *MapLongitudeBounds) {
                                continue;
                            }
                            Map *near_tile = map_loc(x_near, y_near);
                            if ((near_tile->climate & 0xE0) > 0x40 && near_tile->region < 0x40
                                && near_tile->region != region) {
                                landmass++;
                            }
                        }
                    } else {
                        int region_best = -1;
                        for (int i = 0; i < 8; i++) {
                            int x_near = x + RadiusBaseX[i];
                            if (!is_flat) {
                                if (x_near < 0) {
                                    x_near += *MapLongitudeBounds;
                                } else if (x_near >= *MapLongitudeBounds) {
                                    x_near -= *MapLongitudeBounds;
                                }
                            }
                            int y_near = y + RadiusBaseY[i];
                            if (y_near < 0 || y_near >= *MapLatitudeBounds || x_near < 0
                                || x_near >= *MapLongitudeBounds) {
                                continue;
                            }
                            Map *near_tile = map_loc(x_near, y_near);
                            if ((near_tile->climate & 0xE0) <= 0x40) {
                                continue;
                            }
                            int region = near_tile->region;
                            if (region_best < 0) {
                                region_best = region;
                            } else if (region_best != region) {
                                int size = (int)Continents[region].tile_count;
                                int size_best = (int)Continents[region_best].tile_count;
                                if (size_best + size >= (int)*MapArea * 50 / 3200) {
                                    landmass = 1;
                                }
                                if (size > size_best) {
                                    region_best = region;
                                }
                            }
                        }
                    }
                }
                int result = value;
                if (alt <= 1) {
                    result = value - WorldBuilder->deep_water;
                } else if (alt == 2) {
                    if (landmass) {
                        water += 4;
                        water = water < 0 ? 0 : (water > 8 ? 8 : water);
                    }
                    if (value < contour) {
                        result = WorldBuilder->shelf + value;
                    }
                    if (result >= (int)AltNatural[alt + 1]) {
                        result += -((water - 3) * WorldBuilder->shelf / 4);
                    }
                } else if (alt == 3) {
                    if (landmass) {
                        water += 4;
                        water = water < 0 ? 0 : (water > 8 ? 8 : water);
                    }
                    result = (8 - water) * WorldBuilder->plains * root_area / (root_base * 8) + value;
                    int coverage = (int)*MapOceanCoverage;
                    coverage = coverage < 1 ? 1 : (coverage > 2 ? 2 : coverage);
                    if (result < contour) {
                        result += ((((int)WorldBuilder->hills * erosive) / 2) * (8 - water)
                            / (coverage * 8) + WorldBuilder->beach) * root_area / root_base;
                    } else {
                        int water_capped = water < 0 ? 0 : (water > 8 ? 8 : water);
                        result = (result - contour) * (8 - water_capped) * erosive / (coverage * 8)
                            + contour;
                    }
                } else if (alt == 4) {
                    if (value > contour) {
                        result = value * 2 - contour;
                    }
                    result += (int)WorldBuilder->hills * erosive + WorldBuilder->peaks;
                } else {
                    result = (int)WorldBuilder->hills * erosive + value;
                }
                result = (result + contour + 1) / 2;
                if (!erosive && result > 74) {
                    result = 74;
                }
                alt_put_detail(x, y, (uint8_t)result);
            }
            do_all_non_input();
        }
    }
}

/*
Purpose: Set the rockiness of every tile from its mineral value and scatter the energy and mineral
         resource markers: ocean tiles (and tiles that keep fresh water) get energy, land gets
         energy on half the jungle tiles, minerals on high or rainy land, nothing elsewhere.
         Crater and Unity tiles of the fixed landmark sequences are left alone.
Original Offset: 005C32A0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch4.log)
*/
void __cdecl world_rocky() {
    for (int y = 0; y < (int)*MapLatitudeBounds; y++) {
        for (int x = y & 1; x < (int)*MapLongitudeBounds; x += 2) {
            Map *tile = map_loc(x, y);
            uint32_t bit2 = tile->bit2;
            uint32_t code = bit2 >> 24;
            if ((bit2 & (BIT2_CRATER | BIT2_UNK_80000000)) == BIT2_CRATER && code <= 0x14) {
                continue;
            }
            if ((bit2 & (BIT2_UNITY | BIT2_UNK_80000000)) == BIT2_UNITY
                && (code <= 8 || code == 0xA || code == 0xC || code == 0xF || code == 0x10
                    || code == 0x13 || code == 0x14)) {
                continue;
            }
            rocky_set(x, y, (uint8_t)minerals_at(x, y));
            uint32_t alt_bits = tile->climate & 0xE0;
            if ((bit2 & BIT2_FRESH) && alt_bits < ALT_BIT_SHORE_LINE) {
                continue;
            }
            uint32_t bit;
            if (alt_bits < ALT_BIT_SHORE_LINE) {
                bit = BIT_ENERGY_RSC;
            } else if ((bit2 & (BIT2_JUNGLE | BIT2_UNK_80000000)) == BIT2_JUNGLE
                && _rand() % 2 == 0) {
                bit = BIT_ENERGY_RSC;
            } else {
                uint8_t climate = tile->climate;
                if ((climate & 0xE0) <= ALT_BIT_1_LEVEL_ABOVE_SEA && (climate & 0x18) <= RAINFALL_MOIST) {
                    continue;
                }
                bit = BIT_MINERAL_RSC;
            }
            bit_set(x, y, bit, true);
        }
        do_all_non_input();
    }
}

/*
Purpose: Spread the initial fungus: tiles whose noise value falls in one of three bands (whose
         width grows with the native life setting), half of the polar row tiles, and nothing
         on the fixed volcano landmark tiles. Fungus clears terrain improvements and, in the
         expansion, one land tile in forty gets a fungal tower.
Original Offset: 005C3440
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch4.log)
*/
void __cdecl world_fungus() {
    WorldNoise noise;
    noise.init();
    int band = (*MapNativeLifeForms - 1) * WorldBuilder->fungus;
    for (int y = 0; y < (int)*MapLatitudeBounds; y++) {
        for (int x = y & 1; x < (int)*MapLongitudeBounds; x += 2) {
            int value = (noise.at(x << 4, y << 4) * 5 + noise.at(x << 8, y << 8) * 2) * 7 >> 8;
            if (value < 0) {
                value = 0;
            } else if (value > 100) {
                value = 100;
            }
            Map *tile = map_loc(x, y);
            uint32_t bit2 = tile->bit2;
            if ((bit2 & (BIT2_VOLCANO | BIT2_UNK_80000000)) == BIT2_VOLCANO && (bit2 >> 24) <= 8) {
                continue;
            }
            if ((value < 20 - band || value > band * 2 + 25) && (value < 40 - band || value > band * 2 + 45)
                && (value < 60 - band || value > band * 2 + 65)) {
                // outside every band: the polar rows still get fungus on half their tiles
                if ((y != 0 && y != (int)*MapLatitudeBounds - 1) || _rand() % 2 == 0) {
                    continue;
                }
            }
            if ((tile->val3 & 0xC0) < 0x80 || (tile->climate & 0xE0) < ALT_BIT_SHORE_LINE) {
                tile->bit |= BIT_FUNGUS;
                tile->bit &= ~*TerrainBitsFungusClears;
                int roll = _rand();
                if (roll % 40 == 0 && (tile->climate & 0xE0) >= ALT_BIT_SHORE_LINE && *ExpansionEnabled) {
                    veh_init(BSC_FUNGAL_TOWER, 0, x, y);
                }
            }
        }
        do_all_non_input();
    }
}

/*
Purpose: Build out the world river beds.
Original Offset: 005C3680
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_riverbeds() {
    for (uint32_t i = 0; i < *MapArea; i++) {
        (*MapTiles)[i].bit &= ~(BIT_RIVERBED);
    }
    uint32_t riverbed_count = 0;
    uint32_t max_riverbeds = (*MapArea * ((4 - *MapOceanCoverage) * (WorldBuilder->rivers_base 
        + *MapCloudCover * WorldBuilder->rivers_rain_mod) / 3)) / 3200;
    for (uint32_t i = 0; i < 4000 && riverbed_count < max_riverbeds; i++) {
        int x = rnd(*MapLongitudeBounds, NULL);
        int y = rnd(*MapLatitudeBounds, NULL);
        if (x & 1) {
            x--;
        }
        if (y & 1) {
            x++;
        }
        if (!is_ocean(x, y) && !(bit_at(x, y) & BIT_RIVERBED)) {
            uint32_t search_alt = 0;
            int x_search = -1;
            int y_search = -1;
            BOOL has_riverbed = false;
            for (int j = 0; j < RadiusRange[4]; j++) {
                int x_radius = xrange(x + RadiusOffsetX[j]);
                int y_radius = y + RadiusOffsetY[j];
                if (on_map(x_radius, y_radius)) {
                    if (bit_at(x_radius, y_radius) & BIT_RIVERBED) {
                        has_riverbed = true; // riverbed already in search area: skip candidate
                        break;
                    }
                    if (j < RadiusRange[3]) { // limit scope of where riverbeds can be in search
                        uint32_t alt = alt_at(x_radius, y_radius);
                        if (alt > search_alt) {
                            search_alt = alt;
                            x_search = x_radius; // replaced storing j offset with radius values
                            y_search = y_radius;
                        }
                    }
                }
            }
            if (!has_riverbed && x_search >= 0) {
                bit_set(x_search, y_search, BIT_RIVERBED, true);
                riverbed_count++;
            }
        }
    }
}

// The four diagonal steps a river takes (0x66EF28, 0x66EF3C); direction d ^ 2 goes back.
static const int RiverStepX[] = { 1, 1, -1, -1 };
static const int RiverStepY[] = { -1, 1, 1, -1 };

/*
Purpose: Wrap a column on a round map the way world_rivers() does.
Original Offset: n/a (inlined in world_rivers())
Return Value: Column
Status: Complete
*/
static int river_wrap(int x, BOOL is_flat) {
    if (!is_flat) {
        if (x < 0) {
            x += *MapLongitudeBounds;
        } else if (x >= *MapLongitudeBounds) {
            x -= *MapLongitudeBounds;
        }
    }
    return x;
}

/*
Purpose: Lay the rivers. Every land tile marked as a river source (BIT_RIVERBED) starts one, with
         its own random stream (seeded from the tile) and a random first direction. The river
         walks diagonally downhill or level, never back the way it came, choosing among the steps
         by weight: lower contour weighs more, the direction it was heading (with a random swing)
         gets a bonus, an uphill step is only allowed to the lowest contour seen so far, a step
         next to the sea is halved early in a level run, and a step touching its own course is
         cut to an eighth (allowed once). It stops when it reaches an existing river, a source with
         another source beside it, a thermal bore, a dead end or after 1000 steps; where it ends on
         land without joining a river or a thermal bore, the last tile becomes a lake (unless a
         unit or base of a faction is there). Contours raised along the way are lowered to the
         course. The tiles of the course get BIT_RIVER; BIT2_UNK_800000 marks the course while it
         is traced.
Original Offset: 005C38B0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch29.log)
*/
void __cdecl world_rivers() {
    for (uint32_t i = 0; i < *MapArea; i++) {
        (*MapTiles)[i].bit &= ~(BIT_RIVER | BIT_RIVER_LAKE);
    }
    for (int y = 0; y < *MapLatitudeBounds; y++) {
        for (int x = y & 1; x < *MapLongitudeBounds; x += 2) {
            Map *source = map_loc(x, y);
            if (!(source->bit & BIT_RIVERBED) || (source->climate & 0xE0) < 0x60) {
                continue;
            }
            for (uint32_t i = 0; i < *MapArea; i++) {
                (*MapTiles)[i].bit2 &= ~BIT2_UNK_800000;
            }
            random_reseed(y * 47 + x * 31);
            int dir = random(0, 4);
            int x_cur = x;
            int y_cur = y;
            int alt_cur = source->climate >> 5;
            source->bit2 |= BIT2_UNK_800000;
            int contour_cur = source->contour;
            int steps = 0;
            BOOL on_course = false; // the step taken touches the river's own course
            BOOL lake = true;
            int step_count = 1;
            int run = 1; // steps at the same altitude
            for (;;) {
                if (map_loc(x_cur, y_cur)->bit & BIT_THERMAL_BORE) {
                    lake = false;
                    break;
                }
                int swing = random(0, 2);
                int dir_preferred = (swing + (dir - (step_count & 1))) & 3;
                int dir_back = dir ^ 2;
                int choices = 0;
                int weight_total = 0;
                int contour_up = 9999; // lowest contour of an uphill step so far
                int weight_up = 8;
                int weights[3], courses[3], dirs[3];
                BOOL joined = false;
                for (int d = 0; d < 4; d++) {
                    if (d == dir_back) {
                        continue;
                    }
                    BOOL is_flat = *MapIsFlat & 1;
                    int x_step = river_wrap(x_cur + RiverStepX[d], is_flat);
                    int y_step = y_cur + RiverStepY[d];
                    if (y_step < 0 || y_step >= *MapLatitudeBounds || x_step < 0
                        || x_step >= *MapLongitudeBounds) {
                        continue;
                    }
                    Map *step = map_loc(x_step, y_step);
                    if (step->bit & BIT_RIVER) {
                        joined = true;
                        break;
                    }
                    if (step->bit & BIT_RIVERBED) {
                        int sources = 0;
                        for (int e = 0; e < 4; e++) {
                            int x_near = river_wrap(x_step + RiverStepX[e], is_flat);
                            int y_near = y_step + RiverStepY[e];
                            if (y_near >= 0 && y_near < *MapLatitudeBounds && x_near >= 0
                                && x_near < *MapLongitudeBounds
                                && (map_loc(x_near, y_near)->bit & BIT_RIVERBED)) {
                                sources++;
                            }
                        }
                        if (sources > 1) {
                            joined = true;
                            break;
                        }
                    }
                    uint32_t climate = step->climate;
                    if ((int)(climate >> 5) > alt_cur) {
                        continue;
                    }
                    int contour = step->contour;
                    int weight = contour_cur - contour + 2;
                    weight = (weight < 1 ? 1 : (weight > 9999 ? 9999 : weight)) * 32;
                    if (d == dir_preferred) {
                        weight += 31;
                    }
                    if (contour > contour_cur) {
                        if (contour > contour_up) {
                            continue;
                        }
                        contour_up = contour;
                        weight = weight_up++;
                    }
                    BOOL course = false;
                    int higher = 0;
                    BOOL skip = false;
                    for (int e = 0; e < 4 && !skip; e++) {
                        if (e == (d ^ 2)) {
                            continue;
                        }
                        int x_near = river_wrap(x_step + RiverStepX[e], is_flat);
                        int y_near = y_step + RiverStepY[e];
                        if (y_near < 0 || y_near >= *MapLatitudeBounds || x_near < 0
                            || x_near >= *MapLongitudeBounds) {
                            continue;
                        }
                        Map *near_tile = map_loc(x_near, y_near);
                        uint32_t alt_near = near_tile->climate & 0xE0;
                        if (alt_near < 0x60 && (climate & 0xE0) >= 0x60 && run < 5) {
                            weight /= 2;
                        }
                        if (near_tile->contour > contour) {
                            higher++;
                            if (near_tile->bit & BIT_RIVER) {
                                skip = true;
                                continue;
                            }
                        }
                        if (near_tile->bit2 & BIT2_UNK_800000) {
                            if (on_course || alt_near < 0x60) {
                                skip = true;
                                continue;
                            }
                            course = true;
                            weight /= 8;
                        }
                    }
                    if (skip || !weight) {
                        continue;
                    }
                    if (higher == 3) {
                        weight = 1;
                    }
                    weight_total += weight;
                    weights[choices] = weight;
                    courses[choices] = course;
                    dirs[choices] = d;
                    choices++;
                }
                if (joined) {
                    lake = false;
                    break;
                }
                if (!choices) {
                    break;
                }
                int pick = random(0, weight_total);
                dir = 0; // as in the original, when no choice is picked
                for (int i = 0; i < choices; i++) {
                    if (pick < weights[i]) {
                        on_course = courses[i];
                        dir = dirs[i];
                        break;
                    }
                    pick -= weights[i];
                }
                x_cur = river_wrap(x_cur + RiverStepX[dir], *MapIsFlat & 1);
                y_cur += RiverStepY[dir];
                Map *tile = map_loc(x_cur, y_cur);
                int alt = tile->climate >> 5;
                if (alt != alt_cur) {
                    run = 0;
                }
                alt_cur = alt;
                int contour = tile->contour;
                if (contour > contour_cur) {
                    contour = (int)ElevDetail[alt];
                    if (contour_cur - 1 > contour) {
                        contour = contour_cur - 1;
                    }
                    alt_put_detail(x_cur, y_cur, (uint8_t)contour);
                }
                contour_cur = contour;
                if (++steps >= 1000) {
                    break;
                }
                map_loc(x_cur, y_cur)->bit2 |= BIT2_UNK_800000;
                step_count++;
                run++;
            }
            if (alt_cur >= 3) {
                Map *tile = map_loc(x_cur, y_cur);
                if (!(tile->bit & BIT_RIVER) && lake && (!(tile->bit & (BIT_BASE_IN_TILE | BIT_VEH_IN_TILE))
                    || (tile->val2 & 0xF) >= 8)) {
                    tile->bit |= BIT_RIVER_LAKE;
                }
            }
            for (uint32_t i = 0; i < *MapArea; i++) {
                if ((*MapTiles)[i].bit2 & BIT2_UNK_800000) {
                    (*MapTiles)[i].bit |= BIT_RIVER;
                }
            }
        }
        do_all_non_input();
    }
    for (uint32_t i = 0; i < *MapArea; i++) {
        (*MapTiles)[i].bit2 &= ~BIT2_UNK_800000;
    }
}

/*
Purpose: Turn the contour map into altitude levels: every tile gets its natural altitude, then the
         levels from mountain tops down to ocean shelf are re-applied through world_alt_set() so
         the neighbours settle. In the scenario editor with omniscient view the tiles are drawn
         as they change.
Original Offset: 005C3F70
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch4.log)
*/
void __cdecl world_shorelines() {
    BOOL draw = (*GameState & STATE_OMNISCIENT_VIEW) && *WorldBuildUnk1;
    for (int y = 0; y < (int)*MapLatitudeBounds; y++) {
        for (int x = y & 1; x < (int)*MapLongitudeBounds; x += 2) {
            uint32_t alt = alt_natural(x, y);
            alt_set(x, y, alt);
            if (draw) {
                if (alt > 2) {
                    int temperature = alt - 3;
                    if (temperature < 0) {
                        temperature = 0;
                    } else if (temperature > 2) {
                        temperature = 2;
                    }
                    temp_set(x, y, (uint8_t)temperature);
                }
                draw_tile(x, y, 2);
            }
        }
        do_all_non_input();
    }
    for (uint32_t alt = ALT_3_LEVELS_ABOVE_SEA; alt > 1; alt--) {
        for (int y = 0; y < (int)*MapLatitudeBounds; y++) {
            for (int x = y & 1; x < (int)*MapLongitudeBounds; x += 2) {
                if ((uint32_t)(map_loc(x, y)->climate >> 5) == alt) {
                    world_alt_set(x, y, alt, true);
                }
            }
            do_all_non_input();
        }
    }
    if (draw) {
        MapWin_clear_terrain(ConsoleVar, 0);
        draw_map(1);
    }
}

/*
Purpose: Determine if there are any issues with how the world continents are set up.
Original Offset: 005C40F0
Return Value: Are there any issues with the world continents? true/false
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
BOOL __cdecl world_validate() {
    Paths->continents();
    uint32_t val1 = 0; // rename to better var descriptions
    uint32_t val2 = 0; // rename to better var descriptions
    for (uint32_t region = 1; region < MaxRegionLandNum; region++) {
        if (!bad_reg(region)) {
            uint32_t tile_count = Continents[region].tile_count;
            if (tile_count < val2) {
                if (tile_count >= val1) {
                    val1 = tile_count;
                }
            } else {
                val1 = val2;
                val2 = tile_count;
            }
        }
    }
    return val1 < ((*MapLandCoverage == 1) ? ((val2 * 2) / 3) : val2 / 2);
}

/*
Purpose: Set up the world temperature.
Original Offset: 005C4170
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_temperature() {
    random_reseed(*MapRandSeed + 17);
    int temp_heat = *MapLatitudeBounds / (int)WorldBuilder->solar_energy;
    int thermal_banding = *MapLatitudeBounds / (int)WorldBuilder->thermal_band;
    int thermal_deviance = *MapLatitudeBounds / (int)WorldBuilder->thermal_deviance;
    int global_warming = *MapLatitudeBounds / (int)WorldBuilder->global_warming;
    for (int y = 0; y < *MapLatitudeBounds; y++) {
        for (int x = y & 1; x < *MapLongitudeBounds; x += 2) {
            if ((bit2_at(x, y) & (BIT2_UNK_80000000 | BIT2_CRATER)) != BIT2_CRATER
                || (int)(bit2_at(x, y) & 0xFF000000) > 0x14FFFFFF) { // code >= 21, signed
                int rand_seed = random(0, thermal_deviance * 2);
                int rand_orbit = random(0, *MapPlanetaryOrbit + 1);
                int val1 = (*MapLatitudeBounds / 2) - rand_seed - y + thermal_deviance;
                if (val1 < 0) { // abs?
                    val1 = rand_seed - (*MapLatitudeBounds / 2) - thermal_deviance + y;
                }
                int orbit = (int)*MapPlanetaryOrbit;
                int val2 = (thermal_banding / 2 + (val1 - temp_heat * (orbit - 1)
                    - *MapSeaLevelCouncil * global_warming) * 2) / thermal_banding; // signed
                int temperature = (val2 > 2) ? ((val2 <= 9) + 1) : 3;
                int alt = alt_at(x, y);
                for (int i = 0; i < RadiusRange[1]; i++) {
                    int x_radius = xrange(x + RadiusOffsetX[i]);
                    int y_radius = y + RadiusOffsetY[i];
                    if (on_map(x_radius, y_radius) && bit_at(x_radius, y_radius) & BIT_THERMAL_BORE
                        && (alt <= ALT_1_LEVEL_ABOVE_SEA || alt_at(x_radius, y_radius) >= alt)) {
                        temperature++;
                        break;
                    }
                }
                if (temperature > 1) {
                    if (alt <= ALT_2_LEVELS_ABOVE_SEA) {
                        // == ALT_2_LEVELS_ABOVE_SEA, simplify?
                        if (alt > ALT_1_LEVEL_ABOVE_SEA && !rand_orbit) {
                            temperature--;
                        }
                    } else if (*MapPlanetaryOrbit < 2 || !random(0, 2)) {
                        temperature--;
                    }
                }
                temp_set(x, y, (uint8_t)range(temperature - 1, 0, 2));
            }
        }
        do_all_non_input();
    }
}

/*
Purpose: Make a tile one step wetter (arid to moist, moist to rainy; rainy stays rainy).
Original Offset: 005C4410
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch19.log)
*/
void __cdecl world_UNK1(uint32_t x, uint32_t y) {
    uint32_t rainfall = ((map_loc(x, y)->climate & (RAINFALL_RAINY | RAINFALL_MOIST)) >> 3) + 1;
    climate_set(x, y, (uint8_t)(rainfall < 3 ? rainfall : 2));
}

/*
Purpose: Recompute the rainfall of every tile. Pass one marks the air flow in each tile's unk_1
         byte: condensers wet their surroundings (1, or 4 when already wet), ocean breathes moisture
         eastward (0x80) and dries the tiles upwind of taller water (0x10 cleared), forests on low
         land wet their eastern neighbours (0x40), and hills and mountains cast rain shadows (0x20
         to the west, 0x10 to the east) as far as the cloud mass rules allow. Pass two sweeps each
         row west to east carrying a moisture state across the tiles and writes the rainfall level,
         remembering the first base of the faction at the controls near a change (and the owner
         of a base that got drier). Pass three turns arid tiles next to rainy land moist.
Original Offset: 005C4470
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch4.log)
*/
void __cdecl world_rainfall() {
    int faction_id_current = *FactionIDCurrent;
    *MapRainfallChangeBaseID = -1;
    *MapRainfallChangeFactionID = -1;
    for (uint32_t i = 0; i < *MapArea; i++) {
        (*MapTiles)[i].unk_1 = 0;
    }
    int lat = *MapLatitudeBounds;
    int lon = *MapLongitudeBounds;
    int cloud = *MapCloudCover;
    for (int y = 0; y < lat; y++) {
        for (int x = y & 1; x < lon; x += 2) {
            Map *tile = map_loc(x, y);
            if (tile->bit & BIT_CONDENSER) {
                for (int i = 0; i < 9; i++) {
                    int x_radius = xrange(x + RadiusOffsetX[i]);
                    int y_radius = y + RadiusOffsetY[i];
                    if (y_radius >= 0 && y_radius < lat && x_radius >= 0 && x_radius < lon) {
                        Map *wet = map_loc(x_radius, y_radius);
                        wet->unk_1 |= (wet->unk_1 & 1) ? 4 : 1;
                    }
                }
            }
            if ((tile->climate & 0xE0) < ALT_BIT_SHORE_LINE) {
                for (int k = 0; k < 3; k++) {
                    int x_from = xrange(x - 2 * k);
                    int x_to = xrange(x - 2 * k - 2);
                    if (x_to >= 0 && x_to < lon && x_from >= 0 && x_from < lon
                        && (map_loc(x_from, y)->climate >> 5) <= (map_loc(x_to, y)->climate >> 5)
                        && k <= cloud) {
                        map_loc(x_to, y)->unk_1 &= ~0x10;
                    }
                }
                int x_east = xrange(x + 2);
                if (x_east >= 0 && x_east < lon) {
                    map_loc(x_east, y)->unk_1 |= 0x80;
                }
                if (cloud > 0) {
                    x_east = xrange(x + 4);
                    if (x_east >= 0 && x_east < lon) {
                        Map *east = map_loc(x_east, y);
                        if ((east->climate & 7) > 1 || cloud > 1) {
                            east->unk_1 |= 0x80;
                            if (cloud > 1) {
                                x_east = xrange(x + 6);
                                if (x_east >= 0 && x_east < lon && (map_loc(x_east, y)->climate & 7)) {
                                    map_loc(x_east, y)->unk_1 |= 0x80;
                                }
                            }
                        }
                    }
                }
            }
            uint32_t alt = tile->climate >> 5;
            if (alt < 5) {
                if (alt > 2 && (tile->bit & BIT_FOREST)) {
                    for (int y_radius = y - 1; y_radius <= y + 1; y_radius++) {
                        int x_radius = xrange(x + 1);
                        if (y_radius >= 0 && y_radius < lat && x_radius >= 0 && x_radius < lon) {
                            map_loc(x_radius, y_radius)->unk_1 |= 0x40;
                        }
                    }
                    int x_radius = xrange(x + 2);
                    if (x_radius >= 0 && x_radius < lon) {
                        map_loc(x_radius, y)->unk_1 |= 0x40;
                    }
                }
            } else {
                int reach = (int)(alt < 6 ? WorldBuilder->cloudmass_hills : WorldBuilder->cloudmass_peaks)
                    + (cloud - 1) * (int)WorldBuilder->rainfall_coeff;
                for (int j = 1; j <= reach; j++) {
                    int x_west = xrange(x - 2 * j);
                    if (x_west >= 0 && x_west < lon) {
                        Map *shade = map_loc(x_west, y);
                        if ((shade->climate & 0xE0) < ALT_BIT_SHORE_LINE
                            || alt <= (uint32_t)(shade->climate >> 5)) {
                            break;
                        }
                        shade->unk_1 |= 0x20;
                    }
                }
                for (int j = 1; j <= reach; j++) {
                    int x_east = xrange(x + 2 * j);
                    if (x_east >= 0 && x_east < lon) {
                        Map *shade = map_loc(x_east, y);
                        if ((shade->climate & 0xE0) < ALT_BIT_SHORE_LINE
                            || alt <= (uint32_t)(shade->climate >> 5)) {
                            break;
                        }
                        shade->unk_1 |= 0x10;
                    }
                }
            }
        }
        do_all_non_input();
    }
    // the moisture state carried from tile to tile (and row to row)
    int shadow = 0;
    int wet = 0;
    int dry_run = 0;
    for (int y = 0; y < lat; y++) {
        for (int x = y & 1; x < lon; x += 2) {
            Map *tile = map_loc(x, y);
            if (y < 1 || y >= lat - 1) {
                tile->climate &= 0xE7;
                continue;
            }
            uint32_t bit2 = tile->bit2;
            uint8_t climate = tile->climate;
            uint32_t temperature = climate & 7;
            uint32_t alt = climate >> 5;
            uint8_t flow = tile->unk_1;
            int rain = 0;
            if ((bit2 & (BIT2_DUNES | BIT2_UNK_80000000)) == BIT2_DUNES
                || (bit2 & (BIT2_MESA | BIT2_UNK_80000000)) == BIT2_MESA) {
                wet = 1;
            }
            BOOL shadowed = (flow & 0x20) != 0;
            if (shadowed) {
                dry_run = 0;
                wet = 1;
            }
            if (flow & 0x80) {
                wet = temperature != 1 && temperature != 0;
                shadow = 0;
                dry_run = 0;
                rain = cloud > 0;
            }
            if (flow & 0x10) {
                if (wet) {
                    wet--;
                }
                rain--;
            }
            if ((flow & 0x40) && alt < 5) {
                shadow = 0;
                dry_run = 0;
                if (rain < 0) {
                    rain = 0;
                }
            }
            if (alt < 4) {
                BOOL decay = false;
                if (alt < 3) {
                    dry_run = 0;
                    wet = 1;
                    decay = shadow != 0;
                } else if (!wet) {
                    decay = shadow != 0;
                } else if (!shadow) {
                    dry_run++;
                    if (dry_run > cloud * 4 + 2) {
                        wet = 0;
                        dry_run = 0;
                    }
                } else {
                    decay = true;
                }
                if (decay) {
                    shadow -= 1 + cloud;
                    if (shadow < 1) {
                        shadow = 0;
                        dry_run = 0;
                        if (cloud || alt < 3) {
                            wet = 1;
                        }
                    }
                }
            } else {
                shadow++;
                if (shadow > (int)WorldBuilder->rainfall_coeff * cloud * 4 + 1 && wet) {
                    wet--;
                }
            }
            int rainfall = shadowed ? 1 : 0;
            if (tile->bit & BIT_RIVER) {
                if ((x >> 1) % 3 != 0 && rain < 0) {
                    rain = 0;
                }
                wet = 1;
                dry_run = 0;
            }
            if (flow & 2) {
                rainfall--;
                wet = 0;
            } else if (wet) {
                rain += wet;
            }
            if ((int)alt > 4 + (temperature != 0)) {
                if ((int)(cloud - alt + 5 + temperature) > 1 && rain > 0) {
                    rainfall++;
                }
            } else {
                rainfall += rain;
            }
            if ((bit2 & (BIT2_JUNGLE | BIT2_UNK_80000000)) == BIT2_JUNGLE && alt >= 3 && alt < 6) {
                int code = abs((int)bit2 >> 24);
                if (code < 0x31) {
                    rainfall += 2;
                } else if (code < 0x51 && alt < 5) {
                    rainfall += 2;
                } else {
                    rainfall++;
                }
            }
            if ((bit2 & (BIT2_DUNES | BIT2_UNK_80000000)) == BIT2_DUNES) {
                rainfall = 0;
            }
            if ((bit2 & (BIT2_UNITY | BIT2_UNK_80000000)) == BIT2_UNITY || rainfall < 0) {
                rainfall = 0;
            } else if (rainfall > 2) {
                rainfall = 2;
            }
            if (flow & 1) {
                wet = 1;
                if (rainfall < 2) {
                    rainfall++;
                }
            }
            if (flow & 4) {
                wet = 1;
                if (rainfall < 2) {
                    rainfall++;
                }
            }
            int rainfall_old = (climate & 0x18) >> 3;
            if (*TurnCurrentNum && rainfall_old != rainfall && *MapRainfallChangeBaseID < 0) {
                if (rainfall < rainfall_old
                    && (*MapRainfallChangeFactionID < 0 || !is_human(*MapRainfallChangeFactionID))) {
                    int base_id = base_find(x, y);
                    if (base_id >= 0) {
                        *MapRainfallChangeFactionID = Bases[base_id].faction_id_current;
                    }
                }
                int base_id = base_find(x, y, faction_id_current);
                if (base_id >= 0 && *BaseFindDist < 3) {
                    *MapRainfallChangeBaseID = base_id;
                }
            }
            tile->climate = (climate & 0xE7) | (uint8_t)(rainfall << 3);
        }
        do_all_non_input();
    }
    for (int y = 0; y < lat; y++) {
        for (int x = y & 1; x < lon; x += 2) {
            if (!(map_loc(x, y)->climate & 0x18)) {
                for (int i = 0; i < 8; i++) {
                    int x_radius = xrange(x + RadiusBaseX[i]);
                    int y_radius = y + RadiusBaseY[i];
                    if (y_radius >= 0 && y_radius < lat && x_radius >= 0 && x_radius < lon) {
                        uint8_t climate = map_loc(x_radius, y_radius)->climate;
                        if ((climate & 0xE0) >= ALT_BIT_SHORE_LINE && (climate & 0x18) == RAINFALL_RAINY) {
                            climate_set(x, y, 1);
                            break;
                        }
                    }
                }
            }
        }
        do_all_non_input();
    }
}

/*
Purpose: Rate a tile as a base site (1-15, 0 for unusable) for a land or a sea base: bonus
         resources, landmarks, pods, rivers and good land in the base radius count for it,
         deep ocean, rocky ground, fungus (after turn 150) and bases in the outer ring count
         against it; the wrong kind of terrain for the base type costs more. Fungus, monolith
         and rocky center tiles rate zero, as does a mismatched tile near too many bases.
Original Offset: 005C4FD0
Return Value: Site rating
Status: Complete (verified vs original: docs/regression-runs/2026-10-16-council-batch8.log)
*/
int __cdecl world_site(int x, int y, uint32_t sea) {
    int lat = *MapLatitudeBounds, lon = *MapLongitudeBounds;
    Map *center = map_loc(x, y);
    BOOL center_ocean = (center->climate & 0xE0) < ALT_BIT_SHORE_LINE;
    int bases_near = 0;
    for (int i = 21; i < 49; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (y_radius < 0 || y_radius >= lat || x_radius < 0 || x_radius >= lon) {
            continue;
        }
        Map *tile = map_loc(x_radius, y_radius);
        if (!(tile->bit & BIT_BASE_IN_TILE)) {
            continue;
        }
        if (i >= 25) {
            bases_near++;
        } else if (center_ocean && (tile->climate & 0xE0) < ALT_BIT_SHORE_LINE) {
            bases_near += 4;
        } else {
            bases_near += 2;
        }
    }
    int score = 0, ocean = 0, deep = 0, big_sea = 0, rocky = 0, fungus = 0, rivers = 0;
    int arable = 0, improved = 0;
    Map *last = NULL;
    for (int i = 0; i <= 20; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (y_radius < 0 || y_radius >= lat || x_radius < 0 || x_radius >= lon) {
            continue;
        }
        Map *tile = map_loc(x_radius, y_radius);
        last = tile;
        int weight = i ? 1 : 0;
        if ((tile->climate & 0xE0) < ALT_BIT_SHORE_LINE) {
            ocean++;
            if ((tile->climate & 0xE0) < ALT_BIT_OCEAN_SHELF) {
                deep++;
            }
            if (i < 9 && (int)Continents[tile->region].tile_count > 0x31) {
                big_sea = 1;
            }
            if (i && bonus_at(x_radius, y_radius, 1)
                && (!(tile->bit & BIT_FUNGUS) || (tile->climate & 0xE0) < ALT_BIT_OCEAN_SHELF)) {
                score++;
            }
            continue;
        }
        uint32_t rockiness = tile->val3 >> 6;
        if (rockiness && i && rockiness > 1) {
            rocky++;
        }
        if (!(tile->bit & BIT_FUNGUS)) {
            if (bonus_at(x_radius, y_radius, 1)) {
                score += weight * 3 - 1;
            }
            uint32_t bit2 = tile->bit2;
            int code = (int)(bit2 & 0xFF000000);
            if ((bit2 & (BIT2_VOLCANO | BIT2_UNK_80000000)) == BIT2_VOLCANO && code < 0x9000000) {
                score += weight * 2 - 1;
            }
            if ((bit2 & (BIT2_CRATER | BIT2_UNK_80000000)) == BIT2_CRATER && code < 0x9000000) {
                score += weight * 3 - 1;
            }
            if ((bit2 & (BIT2_URANIUM | BIT2_UNK_80000000)) == BIT2_URANIUM) {
                score += weight * 4 - 3;
            }
            if ((bit2 & (BIT2_JUNGLE | BIT2_UNK_80000000)) == BIT2_JUNGLE
                && (tile->climate & 0xE0) >= ALT_BIT_SHORE_LINE) {
                score += 2;
            }
            if (i && goody_at(x_radius, y_radius)) {
                score++;
            }
        }
        uint32_t rainfall = (tile->climate & 0x18) >> 3;
        int altitude = (tile->climate >> 5) - 2;
        if (altitude < 0) {
            altitude = 0;
        }
        if (rockiness == 2 || (tile->bit2 & (BIT2_VOLCANO | BIT2_UNK_80000000)) == BIT2_VOLCANO) {
            rainfall = 0;
        }
        uint32_t bit = tile->bit;
        BOOL has_fungus = (bit & BIT_FUNGUS) != 0;
        if (has_fungus) {
            fungus++;
        }
        if (bit & BIT_BASE_RADIUS) {
            continue;
        }
        BOOL good = (bit & (BIT_FOREST | BIT_MONOLITH)) != 0;
        if (!good && rainfall && rockiness == 1) {
            if (altitude && !has_fungus) {
                good = true;
            } else {
                arable += weight * 2 - 1;
                if (rainfall > 1 && !has_fungus) {
                    arable += weight * 2 - 1;
                }
            }
        }
        if (good) {
            improved += weight * 2 - 1;
            if ((bit & BIT_MONOLITH) && !(bit & BIT_FOREST) && (rainfall > 1 || altitude > 1)) {
                improved += weight * 3 - 2;
            }
        }
        if (bit & BIT_RIVER) {
            rivers++;
        }
    }
    score += rivers / 4;
    if (big_sea) {
        score += sea ? -2 : 4;
    }
    if (last->bit & (BIT_UNK_40000000 | BIT_UNK_4000)) {
        score += 4;
    }
    if (!sea) {
        score += -((ocean - big_sea) / 2) - deep / 2;
    } else {
        score += deep / 2 + (ocean - big_sea) / 2;
    }
    int total = (arable + 1) / 3 + score + (improved * 3 + 6) / 4;
    if (*TurnCurrentNum > 150) {
        total -= fungus / 3;
    }
    uint32_t mismatch = (uint32_t)center_ocean != sea;
    total += -(int)((mismatch + 1) * bases_near) - rocky / 4;
    uint32_t center_bit = center->bit;
    if (((center_bit & BIT_FUNGUS) && (center->climate & 0xE0) >= ALT_BIT_OCEAN_SHELF)
        || (center_bit & BIT_MONOLITH)) {
        return 0;
    }
    if ((center->val3 & 0xC0) > 0x40 && (center->climate & 0xE0) >= ALT_BIT_SHORE_LINE) {
        return 0;
    }
    if (mismatch && bases_near > 3) {
        return 0;
    }
    if (total < 1) {
        return 1;
    }
    return total > 15 ? 15 : total;
}

/*
Purpose: Analysis of the world map.
Original Offset: 005C55C0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_analysis() {
    for (uint32_t i = 0; i < MaxContinentNum; i++) {
        Continents[i].open_terrain = 0;
    }
    for (int y = 0; y < *MapLatitudeBounds; y++) {
        for (int x = y & 1; x < *MapLongitudeBounds; x += 2) {
            uint32_t region = region_at(x, y);
            BOOL is_ocean_tile = is_ocean(x, y);
            if (!is_ocean_tile && climate_at(x, y) != RAINFALL_ARID 
                && rocky_at(x, y) < TERRAIN_BIT_ROCKY
                && (!(bit_at(x, y) & BIT_FUNGUS) || altitude_at(x, y) < ALT_BIT_OCEAN_SHELF)) {
                Continents[region].open_terrain++;
            }
            bit_set(x, y, BIT_UNK_40000000 | BIT_UNK_4000, false); // verify map is being iterated 
            if (!is_ocean_tile) {                                  // correctly for all bit_*
                int search_val = -1;
                uint32_t count_val1 = 0;
                uint32_t count_val2 = 0;
                for (uint32_t i = 0; i < 20; i++) {
                    int x_radius = xrange(x + RadiusOffsetX[i + 1]);
                    int y_radius = y + RadiusOffsetY[i + 1];
                    if (on_map(x_radius, y_radius)) {
                        BOOL is_ocean_radius = is_ocean(x_radius, y_radius);
                        if (i >= 8) { // outer ring (offsets 9..20)
                            if (is_ocean_radius != search_val || i == 8) {
                                search_val = is_ocean_radius;
                                count_val1++;
                            }
                            uint32_t region_radius;
                            if (y_radius > 4 && y_radius < ((int)*MapLatitudeBounds - 4)
                                && (region_radius = region_at(x_radius, y_radius),
                                    region_radius < MaxRegionLandNum && region != region_radius
                                    && Continents[region].tile_count >= 40
                                    && Continents[region_radius].tile_count >= 40)) {
                                bit_set(x, y, BIT_UNK_4000, true); // here
                            }
                        } else {
                            if (is_ocean_radius != search_val) {
                                search_val = is_ocean_radius;
                                count_val2++;
                                if (count_val2 >= 4 && Continents[region].tile_count >= 80) {
                                    bit_set(x, y, BIT_UNK_40000000, true); // here
                                }
                            }
                        }
                    }
                }
                if (count_val1 < 4) {
                    bit_set(x, y, BIT_UNK_40000000, false); // here
                }
            }
        }
        do_all_non_input();
    }
    for (uint32_t i = 0; i < *MapArea; i++) {
        (*MapTiles)[i].val2 &= 0xF; // clear map sites
    }
}

/*
Purpose: Set the default altitude details for the specified tile.
Original Offset: 005C58C0
Return Value: n/a
Status: Complete
*/
void __cdecl world_alt_put_detail(uint32_t x, uint32_t y) {
    alt_put_detail(x, y, (uint8_t)AltNatural[3]);
}

/*
Purpose: Set up the world polar caps.
Original Offset: 005C58E0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/sweep-2026-10-16/ (13 saves))
*/
void __cdecl world_polar_caps() {
    for (int x = 0; x < *MapLongitudeBounds; x += 2) {
        world_alt_put_detail(x, 0);
        world_alt_put_detail(x + 1, *MapLatitudeBounds - 1);
    }
    uint32_t bounds = *MapLongitudeBounds / 16;
    for (uint32_t i = 0; i < bounds; i++) {
        world_alt_put_detail(rnd(*MapLongitude, NULL) * 2, 0);
        world_alt_put_detail(rnd(*MapLongitude, NULL) * 2 + 1, 1);
        world_alt_put_detail(rnd(*MapLongitude, NULL) * 2 + 1, *MapLatitudeBounds - 1);
        world_alt_put_detail(rnd(*MapLongitude, NULL) * 2, *MapLatitudeBounds - 2);
    }
}

/*
Purpose: Redo the climate after the land changed: shorelines, temperature, rivers and rainfall, the
         continents and the world analysis, the territory (not during world building), then a
         fresh random seed from the clock; the map views redraw their terrain unless the game is
         halted. Network checksums are logged before and after.
Original Offset: 005C5A30
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-18-council-batch29.log)
*/
void __cdecl world_climate() {
    do_checksums(2);
    log_say("-- ADJUSTING WORLD CLIMATE --", 0, 0, 0);
    world_shorelines();
    world_temperature();
    world_rivers();
    world_rainfall();
    Paths->continents();
    world_analysis();
    if (!*WorldBuildUnk2) {
        reset_territory();
    }
    random_reseed(timeGetTime());
    if (!*GameHalted) {
        for (int i = 0; i < MaxMapWinNum; i++) {
            if (MapWinList[i]->draw_toggle_a) {
                MapWin_clear_terrain(MapWinList[i], 0);
            }
        }
        WorldWin_clear_terrain(WorldWinVar, 0);
    }
    log_say("-- WORLD CLIMATE COMPLETE --", 0, 0, 0);
    do_checksums(3);
}

/*
Purpose: Set up the world contours.
Original Offset: 005C5AE0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_linearize_contours() {
    for (int y = 0; y < *MapLatitudeBounds; y++) {
        for (int x = y & 1; x < *MapLongitudeBounds; x += 2) {
            uint32_t alt_nat = alt_natural(x, y);
            int elev = (int)ElevDetail[alt_nat];
            int elev_next = (int)ElevDetail[alt_nat + ((alt_nat >= ALT_3_LEVELS_ABOVE_SEA) ? 4 : 1)];
            int nat = (int)AltNaturalDefault[alt_nat]; // the constant table, not the live one
            int nat_next = (int)AltNaturalDefault[alt_nat + 1];
            alt_put_detail(x, y, (uint8_t)(((elev_next - elev) * (alt_detail_at(x, y) - nat))
                / (nat_next - nat) + elev));
        }
        do_all_non_input();
    }
    memcpy_s(AltNatural, sizeof(uint32_t) * 11, ElevDetail, sizeof(uint32_t) * 11); // the arrays, not the DLL's pointers to them
}

/*
Purpose: Determine if the specified tile is near a landmark.
Original Offset: 005C5BD0
Return Value: Is the tile near a landmark? true/false
Status: Complete
*/
BOOL __cdecl near_landmark(int x, int y) {
    for (int i = 0; i < RadiusRange[8]; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius) && code_at(x_radius, y_radius)) {
            return true;
        }
    }
    return false;
}

/*
Purpose: Setup the 'Garland Crater' landmark.
Original Offset: 005C5C70
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_crater(int x, int y) {
    uint32_t loc_attempts = 0;
    if (!on_map(x, y)) {
        do {
            do {
                y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (is_ocean(x, y));
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    }
    world_alt_set(x, y, ALT_OCEAN_TRENCH, true);
    world_alt_set(x, y, ALT_3_LEVELS_ABOVE_SEA, true);
    world_alt_set(x, y, ALT_SHORE_LINE, true);
    world_alt_set(x, y, ALT_OCEAN_SHELF, true);
    world_alt_set(x, y, ALT_SHORE_LINE, true);
    uint32_t temperature = temp_at(x, y);
    for (int i = 0; i < RadiusRange[3]; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius)) {
            bit_set(x_radius, y_radius, BIT_RIVERBED, false);
            bit2_set(x_radius, y_radius, BIT2_CRATER, true);
            code_set(x_radius, y_radius, i);
            if (i < 21) {
                rocky_set(x_radius, y_radius, ROCKINESS_ROLLING);
                temp_set(x_radius, y_radius, (uint8_t)temperature);
            }
        }
    }
    new_landmark(x, y, StringTable->get((int)Natural[LM_CRATER].name));
}

/*
Purpose: Setup the 'Monsoon Jungle' landmark.
Original Offset: 005C5EF0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_monsoon(int x, int y) {
    world_rainfall();
    uint32_t loc_attempts = 0;
    if (!on_map(x, y)) {
        do {
            uint32_t land_count;
            do {
                y = *MapLatitudeBounds / 2 + _rand() % 4 - 2;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                land_count = 0;
                for (int i = 0; i < RadiusRange[5]; i++) {
                    int x_radius = xrange(x + RadiusOffsetX[i]);
                    int y_radius = y + RadiusOffsetY[i];
                    if (on_map(x_radius, y_radius) && !is_ocean(x_radius, y_radius)) {
                        land_count++;
                    }
                }
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (is_ocean(x, y) || !is_coast(x, y, true) || land_count < 40
                || climate_at(x, y) <= RAINFALL_MOIST);
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    }
    for (int i = 0; i < RadiusRange[5]; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (abs(RadiusOffsetY[i]) <= 8 &&  on_map(x_radius, y_radius)) {
            if (i < 21 && is_ocean(x_radius, y_radius)) {
                world_alt_set(x_radius, y_radius, ALT_SHORE_LINE, true);
            }
            bit2_set(x_radius, y_radius, BIT2_JUNGLE, true);
            code_set(x_radius, y_radius, i);
        }
    }
    new_landmark(x, y, StringTable->get((int)Natural[LM_JUNGLE].name));
}

/*
Purpose: Setup the 'New Sargasso' landmark.
Original Offset: 005C6200
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_sargasso(int x, int y) {
    uint32_t loc_attempts = 0;
    BOOL has_skipped = false;
    if (!on_map(x, y)) {
        do {
            do {
                y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (!is_ocean(x, y));
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    }
    for (int i = 0; i < RadiusRange[4]; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius) && (is_ocean(x_radius, y_radius) || i < 21)) {
            world_alt_set(x_radius, y_radius, ALT_OCEAN_SHELF, true);
            bit2_set(x_radius, y_radius, BIT2_SARGASSO, true);
            code_set(x_radius, y_radius, i);
            if (i <= 4) {
                bit_set(x_radius, y_radius, BIT_SUPPLY_POD, true);
            }
            if ((has_skipped || i < RadiusRange[2] || _rand() % 24) 
                && (i < RadiusRange[3] || _rand() % 3)) {
                bit_set(x_radius, y_radius, BIT_FUNGUS, true);
            } else {
                has_skipped = true;
            }
        }
    }
    new_landmark(x, y, StringTable->get((int)Natural[LM_SARGASSO].name));
}

/*
Purpose: Setup 'The Ruins' landmark.
Original Offset: 005C64A0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_ruin(int x, int y) {
    uint32_t loc_attempts = 0;
    if (!on_map(x, y)) {
        do {
            do {
                y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (is_ocean(x, y));
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    }
    for (int i = 0; i < RadiusRange[2]; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius)) {
            if (i < RadiusRange[2] || !is_ocean(x_radius, y_radius)) { // is_ocean check pointless?
                world_alt_set(x_radius, y_radius, ALT_SHORE_LINE, true);
            }
            if (is_ocean(x_radius, y_radius)) {
                if (i && i < 9) {
                    bit_set(x_radius, y_radius, BIT_MONOLITH, true); // in ocean tiles?
                }
            } else {
                bit_set(x_radius, y_radius, BIT_FUNGUS | BIT_SUPPLY_REMOVE, true);
                if (!i) {
                    bit_set(x_radius, y_radius, BIT_FUNGUS, false);
                } else if (i < 9) {
                    bit_set(x_radius, y_radius, BIT_MONOLITH, true);
                }
            }
            bit2_set(x_radius, y_radius, BIT2_RUINS, true);
            code_set(x_radius, y_radius, i);
        }
    }
    new_landmark(x, y + 2, StringTable->get((int)Natural[LM_RUINS].name));
}

/*
Purpose: Setup the 'Great Dunes' landmark.
Original Offset: 005C6740
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_dune(int x, int y) {
    world_rainfall();
    uint32_t loc_attempts = 0;
    uint32_t half_vert_bounds = *MapLatitudeBounds / 2;
    if (!on_map(x, y)) {
        do {
            do {
                y = rnd(half_vert_bounds, NULL) + half_vert_bounds - *MapLatitudeBounds / 4;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (is_ocean(x, y) || climate_at(x, y) != RAINFALL_ARID);
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    }
    for (int i = 0; i < RadiusRange[2]; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius) && (!is_ocean(x_radius, y_radius) || i < 9 || _rand() % 3)) {
            if (i == 2 || i == 7) {
                world_alt_set(x_radius, y_radius, ALT_1_LEVEL_ABOVE_SEA, true);
            } else {
                world_alt_set(x_radius, y_radius, ALT_SHORE_LINE, true);
            }
            bit_set(x_radius, y_radius, BIT_FUNGUS, false);
            bit2_set(x_radius, y_radius, BIT2_DUNES, true);
            rocky_set(x_radius, y_radius, ROCKINESS_FLAT);
            code_set(x_radius, y_radius, i);
        }
    }
    new_landmark(x, y + 2, StringTable->get((int)Natural[LM_DUNES].name));
}

/*
Purpose: Setup the 'Uranium Flats' landmark.
Original Offset: 005C69E0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_diamond(int x, int y) {
    uint32_t loc_attempts = 0;
    if (!on_map(x, y)) {
        do {
            do {
                y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (is_ocean(x, y));
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    }
    for (uint32_t i = 0; i < 21; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius)) {
            if (i < 9 || !is_ocean(x_radius, y_radius) || _rand() % 3) {
                world_alt_set(x_radius, y_radius, ALT_SHORE_LINE, true);
                bit2_set(x_radius, y_radius, BIT2_URANIUM, true);
                rocky_set(x_radius, y_radius, ROCKINESS_FLAT);
                bit_set(x_radius, y_radius, BIT_FUNGUS, false);
                code_set(x_radius, y_radius, i);
            }
        }
    }
    new_landmark(x, y, StringTable->get((int)Natural[LM_URANIUM].name));
}

/*
Purpose: Setup the 'Freshwater Sea' landmark.
Original Offset: 005C6C40
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_fresh(int x, int y) {
    uint32_t region;
    if (on_map(x, y)) {
        region = region_at(x, y);
        if (!is_ocean(x, y)) {
            return;
        }
    } else {
        uint32_t tile_search = 0;
        int region_search = -1;
        for (uint32_t i = MaxLandmarkNum; i < MaxContinentNum; i++) {
            uint32_t tile_count = Continents[i].tile_count;
            if (tile_count >= 3 && tile_count <= 32 && tile_count >= tile_search) {
                tile_search = tile_count;
                region_search = i;
            }
        }
        if (region_search < 0) {
            return;
        }
        region = region_search;
    }
    int x_search = -1;
    BOOL has_set_landmark = false;
    for (int y_it = *MapLatitudeBounds - 1; y_it >= 0 ; y_it--) {
        for (int x_it = y_it & 1; x_it < *MapLongitudeBounds; x_it += 2) {
            if (region_at(x_it, y_it) == region) {
                bit2_set(x_it, y_it, BIT2_FRESH, true);
                if (x_search < 0) {
                    x_search = x_it;
                }
            } else if(!has_set_landmark && x_search >= 0) {
                int x_fresh = (x_search + x_it - 2) / 2;
                new_landmark(((x_fresh ^ y_it) & 1) ^ x_fresh, y_it,
                    StringTable->get((int)Natural[LM_FRESH].name));
                has_set_landmark = true;
            }
        }
    }
}

/*
Purpose: Setup the 'Mount Planet' landmark.
Original Offset: 005C6DB0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_volcano(int x, int y, BOOL is_not_landmark) {
    uint32_t loc_attempts = 0;
    if (!on_map(x, y)) {
        do {
            do {
                y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (is_ocean(x, y));
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    }
    world_alt_set(x, y, ALT_BIT_OCEAN_TRENCH, true); // purpose??
    world_alt_set(x, y, ALT_3_LEVELS_ABOVE_SEA, true);
    for (int i = 0; i < RadiusRange[1]; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius)) {
            bit2_set(x_radius, y_radius, BIT2_VOLCANO, true);
            code_set(x_radius, y_radius, i);
            uint32_t bit = bit_at(x_radius, y_radius) & ~(BIT_SUPPLY_POD | BIT_THERMAL_BORE
                | BIT_ECH_MIRROR | BIT_CONDENSER | BIT_SOIL_ENRICHER | BIT_FARM | BIT_RIVERBED
                | BIT_SOLAR_TIDAL | BIT_FUNGUS | BIT_MINE | BIT_MAGTUBE | BIT_ROAD);
            if (!i) {
                bit &= ~(BIT_UNK_4000000 | BIT_UNK_8000000);
            }
            bit |= BIT_SUPPLY_REMOVE;
            bit_put(x_radius, y_radius, bit);
            rocky_set(x_radius, y_radius, ROCKINESS_ROCKY);
        }
    }
    *MountPlanetX = x;
    *MountPlanetY = y;
    if (!is_not_landmark) {
        new_landmark(x, y + 2, StringTable->get((int)Natural[LM_VOLCANO].name));
    }
}

/*
Purpose: Setup the 'Borehole Cluster' landmark. Added to SMAC in 3.0 patch.
Original Offset: 005C7020
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-world-timepin.log with timeGetTime pinned)
*/
void __cdecl world_borehole(int x, int y) {
    uint32_t loc_attempts = 0;
    if (!on_map(x, y)) {
        do {
            do {
                y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (is_ocean(x, y));
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    }
    uint32_t seed = timeGetTime();
    int val1 = 8;
    int val2 = ((seed % 8) / 3) + 5;
    int val3 = 3 - ((seed % 8) % 3);
    uint32_t val4 = (seed / 8) % 4;
    if (val4 & 2) {
        val2--;
        val3--;
        if (val4 & 1) {
            val2++;
            val1 = 8;
            val3++;
        } else {
            val2--;
            val1 = 6;
            val3--;
        }
        val1 = (val1 + 8) % 8 + 1;
        val2 = (val2 + 8) % 8 + 1;
        val3 = (val3 + 8) % 8 + 1;
    }
    for (int i = 0; i < RadiusRange[1]; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius)) {
            world_alt_set(x_radius, y_radius, ALT_SHORE_LINE, true);
            bit_set(x_radius, y_radius, BIT_SUPPLY_REMOVE, true);
            if (i == val1 || i == val2 || i == val3) {
                bit_set(x_radius, y_radius, BIT_THERMAL_BORE, true);
                bit2_set(x_radius, y_radius, BIT2_BOREHOLE, true);
                code_set(x_radius, y_radius, i);
            }
        }
    }
    int x_offset_val1 = xrange(x + RadiusOffsetX[val1]); // removed from loop
    int y_offset_val1 = y + RadiusOffsetY[val1]; // removed from loop
    for (int i = 0; i < 8; i++) {
        int x_radius = xrange(x_offset_val1 + RadiusOffsetX[i + 1]);
        int y_radius = y_offset_val1 + RadiusOffsetY[i + 1];
        if (on_map(x_radius, y_radius) && is_ocean(x_radius, y_radius)) {
            world_alt_set(x_radius, y_radius, ALT_SHORE_LINE, true);
        }
    }
    int x_offset_val2 = xrange(x + RadiusOffsetX[val2]); // removed from loop
    int y_offset_val2 = y + RadiusOffsetY[val2]; // removed from loop
    for (int i = 0; i < 8; i++) {
        int x_radius = xrange(x_offset_val2 + RadiusOffsetX[i + 1]);
        int y_radius = y_offset_val2 + RadiusOffsetY[i + 1];
        if (on_map(x_radius, y_radius) && is_ocean(x_radius, y_radius)) {
            world_alt_set(x_radius, y_radius, ALT_SHORE_LINE, true);
        }
    }
    int x_offset_val3 = xrange(x + RadiusOffsetX[val3]); // removed from loop
    int y_offset_val3 = y + RadiusOffsetY[val3]; // removed from loop
    for (int i = 0; i < 8; i++) {
        int x_radius = xrange(x_offset_val3 + RadiusOffsetX[i + 1]);
        int y_radius = y_offset_val3 + RadiusOffsetY[i + 1];
        if (on_map(x_radius, y_radius) && is_ocean(x_radius, y_radius)) {
            world_alt_set(x_radius, y_radius, ALT_SHORE_LINE, true);
        }
    }
    bit2_set(x, y, BIT2_BOREHOLE, true);
    new_landmark(x, y, StringTable->get((int)Natural[LM_BOREHOLE].name));
}

/*
Purpose: Setup 'The Manifold Nexus' landmark. Added to SMAC in 4.0 patch.
Original Offset: 005C7540
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_temple(int x, int y) {
    uint32_t loc_attempts = 0;
    if (!on_map(x, y)) {
        do {
            do {
                y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (is_ocean(x, y));
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    }
    for (int i = 0; i < RadiusRange[1]; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius)) {
            world_alt_set(x_radius, y_radius, ALT_SHORE_LINE, true);
            bit_set(x_radius, y_radius, BIT_SUPPLY_REMOVE, true);
            bit2_set(x_radius, y_radius, BIT2_NEXUS, true);
            code_set(x_radius, y_radius, i);
        }
    }
    new_landmark(x, y, StringTable->get((int)Natural[LM_NEXUS].name));
}

/*
Purpose: Setup the 'Unity Wreckage' landmark (SMACX only).
Original Offset: 005C7750
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_unity(int x, int y) {
    if (*ExpansionEnabled) {
        uint32_t loc_attempts = 0;
        if (!on_map(x, y)) {
            do {
                do {
                    y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                    int x_seed = rnd(*MapLongitudeBounds, NULL);
                    x = ((x_seed ^ y) & 1) ^ x_seed;
                    if (++loc_attempts >= 1000) {
                        return;
                    }
                } while (is_ocean(x, y));
            } while (near_landmark(x, y));
            if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
                return;
            }
        }
        x--;
        y--;
        for (int i = 0; i < RadiusRange[2]; i++) {
            int x_radius = xrange(x + RadiusOffsetX[i]);
            int y_radius = y + RadiusOffsetY[i];
            if (on_map(x_radius, y_radius)) {
                world_alt_set(x_radius, y_radius, ALT_1_LEVEL_ABOVE_SEA, true);
            }
        }
        x += 2;
        y += 2;
        for (int i = 0; i < RadiusRange[2]; i++) {
            int x_radius = xrange(x + RadiusOffsetX[i]);
            int y_radius = y + RadiusOffsetY[i];
            if (on_map(x_radius, y_radius)) {
                world_alt_set(x_radius, y_radius, ALT_1_LEVEL_ABOVE_SEA, true);
            }
        }
        x--;
        y--;
        for (int i = 0; i < RadiusRange[2]; i++) {
            int x_radius = xrange(x + RadiusOffsetX[i]);
            int y_radius = y + RadiusOffsetY[i];
            if (on_map(x_radius, y_radius)) {
                rocky_set(x_radius, y_radius, ROCKINESS_FLAT);
                bit_set(x_radius, y_radius, BIT_RIVERBED, false);
                bit_set(x_radius, y_radius, BIT_FUNGUS, false);
                bit2_set(x_radius, y_radius, BIT2_UNITY, true);
                code_set(x_radius, y_radius, i);
                if (!i || i == 8 || i == 10 || i == 19) {
                    bit_set(x_radius, y_radius, BIT_SUPPLY_POD, true);
                } else {
                    bit_set(x_radius, y_radius, BIT_SUPPLY_REMOVE, true);
                }
            }
        }
        new_landmark(x, y, StringTable->get((int)Natural[LM_UNITY].name));
    }
}

/*
Purpose: Setup the 'Fossil Ridge' landmark (SMACX only).
Original Offset: 005C7A80
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_fossil(int x, int y) {
    if (*ExpansionEnabled) { // added check similar to Unity Wreckage
        uint32_t loc_attempts = 0;
        if (!on_map(x, y)) {
            do {
                do {
                    y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                    int x_seed = rnd(*MapLongitudeBounds, NULL);
                    x = ((x_seed ^ y) & 1) ^ x_seed;
                    if (++loc_attempts >= 1000) {
                        return;
                    }
                } while (!is_ocean(x, y));
            } while (near_landmark(x, y));
            if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
                return;
            }
        }
        for (uint32_t i = 0; i < 6; i++) {
            int x_radius = xrange(x + RadiusOffsetX[i]);
            int y_radius = y + RadiusOffsetY[i];
            if (on_map(x_radius, y_radius) && is_ocean(x_radius, y_radius)) {
                world_alt_set(x_radius, y_radius, ALT_OCEAN, true);
                bit2_set(x_radius, y_radius, BIT2_FOSSIL, true);
                code_set(x_radius, y_radius, i);
            }
        }
        new_landmark(x, y, StringTable->get((int)Natural[LM_FOSSIL].name));
    }
}

/*
Purpose: Setup the 'Nessus Canyon' landmark.
Original Offset: 005C7CB0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_canyon(int x, int y) {
    uint32_t loc_attempts = 0;
    if (!on_map(x, y)) {
        do {
            do {
                y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (is_ocean(x, y));
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    } 
    const uint32_t WorldCanyonOffsetValues[12] = { 26, 27, 9, 1, 2, 0, 3, 4, 17, 23, 36, 35 };
    for (uint32_t i = 0; i < 12; i++) {
        int x_radius = xrange(x + RadiusOffsetX[WorldCanyonOffsetValues[i]]);
        int y_radius = y + RadiusOffsetY[WorldCanyonOffsetValues[i]];
        if (on_map(x_radius, y_radius)) {
            world_alt_set(x_radius, y_radius, ALT_1_LEVEL_ABOVE_SEA, true);
            world_alt_set(x_radius, y_radius, ALT_SHORE_LINE, true);
            bit2_set(x_radius, y_radius, BIT2_CANYON, true); // rolled these two into single loop with
            code_set(x_radius, y_radius, WorldCanyonOffsetValues[i]); // the above world_alt_set()
        }
    }
    new_landmark(x, y + 2, StringTable->get((int)Natural[LM_CANYON].name));
}

/*
Purpose: Setup the 'Sunny Mesa' landmark.
Original Offset: 005C7F40
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_mesa(int x, int y) {
    uint32_t loc_attempts = 0;
    if (!on_map(x, y)) {
        do {
            do {
                y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (is_ocean(x, y));
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    }
    for (int i = 0; i < RadiusRange[3]; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius)) {
            world_alt_set(x_radius, y_radius, (i < RadiusRange[2]) + ALT_SHORE_LINE, true);
            bit2_set(x_radius, y_radius, BIT2_MESA, true);
            code_set(x_radius, y_radius, i);
        }
    }
    new_landmark(x, y + 2, StringTable->get((int)Natural[LM_MESA].name));
}

/*
Purpose: Setup the 'Pholus Ridge' landmark.
Original Offset: 005C8150
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_ridge(int x, int y) {
    uint32_t loc_attempts = 0;
    if (!on_map(x, y)) {
        do {
            do {
                y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (is_ocean(x, y));
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    }
    const uint32_t world_ridge_offset_val[13] = { 47, 44, 24, 20, 8, 7, 0, 5, 4, 17, 23, 35, 45 };
    for (uint32_t i = 0; i < 13; i++) { // raise all 13 tiles first ...
        int x_radius = xrange(x + RadiusOffsetX[world_ridge_offset_val[i]]);
        int y_radius = y + RadiusOffsetY[world_ridge_offset_val[i]];
        if (on_map(x_radius, y_radius)) {
            world_alt_set(x_radius, y_radius, ALT_1_LEVEL_ABOVE_SEA, true);
        }
    }
    for (uint32_t i = 0; i < 13; i++) { // ... then mark them
        int x_radius = xrange(x + RadiusOffsetX[world_ridge_offset_val[i]]);
        int y_radius = y + RadiusOffsetY[world_ridge_offset_val[i]];
        if (on_map(x_radius, y_radius)) {
            bit2_set(x_radius, y_radius, BIT2_RIDGE, true);
            code_set(x_radius, y_radius, world_ridge_offset_val[i]);
        }
    }
    new_landmark(x, y + 2, StringTable->get((int)Natural[LM_RIDGE].name));
}

/*
Purpose: Setup the 'Geothermal Shallows' landmark.
Original Offset: 005C83B0
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/2026-10-15-council-full.log)
*/
void __cdecl world_geothermal(int x, int y) {
    uint32_t loc_attempts = 0;
    if (!on_map(x, y)) {
        do {
            do {
                y = rnd(*MapLatitudeBounds - 16, NULL) + 8;
                int x_seed = rnd(*MapLongitudeBounds, NULL);
                x = ((x_seed ^ y) & 1) ^ x_seed;
                if (++loc_attempts >= 1000) {
                    return;
                }
            } while (!is_ocean(x, y));
        } while (near_landmark(x, y));
        if (loc_attempts >= 1000) { // redundant check? would have already returned, remove?
            return;
        }
    }
    BOOL has_skipped = false;
    for (int i = 0; i < RadiusRange[2]; i++) {
        int x_radius = xrange(x + RadiusOffsetX[i]);
        int y_radius = y + RadiusOffsetY[i];
        if (on_map(x_radius, y_radius) && (is_ocean(x_radius, y_radius) || i < 9)) {
            if ((has_skipped || !i || _rand() % 25) && (i < 9 || _rand() % 3)) {
                world_alt_set(x_radius, y_radius, ALT_OCEAN_SHELF, true);
                bit2_set(x_radius, y_radius, BIT2_GEOTHERMAL, true);
                code_set(x_radius, y_radius, i);
            } else {
                has_skipped = true;
            }
        }
    }
    new_landmark(x, y, StringTable->get((int)Natural[LM_GEOTHERMAL].name));
}

/*
Purpose: Set up all the landmarks.
Original Offset: 005C8630
Return Value: n/a
Status: Complete (verified vs original: docs/regression-runs/sweep-2026-10-16/ (13 saves))
*/
void __cdecl world_landmarks() {
    world_monsoon(-1, -1);
    world_crater(-1, -1);
    world_volcano(-1, -1, false);
    world_mesa(-1, -1);
    world_ridge(-1, -1);
    world_diamond(-1, -1);
    world_ruin(-1, -1);
    world_unity(-1, -1); // removed redundant ExpansionEnabled check
    world_fossil(-1, -1); // moved ExpansionEnabled check to inside function
    world_temple(-1, -1);
    world_borehole(-1, -1);
    world_sargasso(-1, -1);
    world_dune(-1, -1);
    world_fresh(-1, -1);
    world_geothermal(-1, -1);
    // upstream added world_canyon(-1, -1) here ("Nessus Canyon" on random maps); the original
    // places none, so it is left out to match it (see docs/regression-testing.md)
    fixup_landmarks();
}

/*
Purpose: Check for any type of zone of control conflicts (base and/or unit).
Original Offset: 005C89F0
Return Value: If ZOC, faction id + 1; Otherwise, 0 (however return seems to be treated as boolean)
Status: Complete
*/
uint32_t __cdecl zoc_any(int x, int y, uint32_t faction_id) {
    for (uint32_t i = 0; i < 8; i++) {
        int x_radius = xrange(x + RadiusBaseX[i]);
        int y_radius = y + RadiusBaseY[i];
        if (on_map(x_radius, y_radius)) {
            int owner = anything_at(x_radius, y_radius);
            if (owner >= 0 && (uint32_t)owner != faction_id
                && !has_treaty(faction_id, owner, DTREATY_PACT)) {
                return owner + 1;
            }
        }
    }
    return 0;
}

/*
Purpose: Check for unit related zone of control conflicts.
Original Offset: 005C8AC0
Return Value: If ZOC, faction id + 1; Otherwise, 0 (however return seems to be treated as boolean)
Status: Complete
*/
uint32_t __cdecl zoc_veh(int x, int y, uint32_t faction_id) {
    uint32_t ret = 0;
    for (uint32_t i = 0; i < 8; i++) {
        int x_radius = xrange(x + RadiusBaseX[i]);
        int y_radius = y + RadiusBaseY[i];
        if (on_map(x_radius, y_radius)) {
            int owner = veh_who(x_radius, y_radius);
            if (owner >= 0 && (uint32_t)owner != faction_id
                && !has_treaty(faction_id, owner, DTREATY_PACT)) {
                owner++;
                if (ret <= (uint32_t)owner) {
                    ret = owner; // any point in continuing after 1st instance of ZOC is found?
                }
            }
        }
    }
    return ret;
}

/*
Purpose: Check for unit related zone of control conflicts taking into account land or ocean.
Original Offset: 005C8BA0
Return Value: If ZOC, faction id + 1; Otherwise, 0 (however return seems to be treated as boolean)
Status: Complete
*/
uint32_t __cdecl zoc_sea(int x, int y, uint32_t faction_id) {
    BOOL is_ocean_tile = is_ocean(x, y);
    for (uint32_t i = 0; i < 8; i++) {
        int x_radius = xrange(x + RadiusBaseX[i]);
        int y_radius = y + RadiusBaseY[i];
        if (on_map(x_radius, y_radius)) {
            int owner = veh_who(x_radius, y_radius);
            if (owner >= 0 && (uint32_t)owner != faction_id
                && is_ocean(x_radius, y_radius) == is_ocean_tile
                && !has_treaty(faction_id, owner, DTREATY_PACT)) {
                for (int veh_id = veh_at(x_radius, y_radius); veh_id >= 0;
                    veh_id = Vehs[veh_id].next_veh_id_stack) {
                    if (Vehs[veh_id].faction_id != faction_id
                        && (Vehs[veh_id].visibility & (1 << faction_id)
                            || (!*IsMultiplayerNet && !(Vehs[veh_id].flags & VFLAG_INVISIBLE)))) {
                        return owner + 1;
                    }
                }
            }
        }
    }
    return 0;
}

/*
Purpose: Check for unit related zone of control conflicts when moving to a non-base tile.
Original Offset: 005C8D40
Return Value: If ZOC, faction id + 1; Otherwise, 0 (however return seems to be treated as boolean)
Status: Complete
*/
uint32_t __cdecl zoc_move(uint32_t x, uint32_t y, uint32_t faction_id) {
    int owner;
    if (!(bit_at(x, y) & BIT_BASE_IN_TILE)
        || ((owner = owner_at(x, y)), owner >= 8 || owner < 0)) {
        return zoc_sea(x, y, faction_id);
    }
    return 0;
}

/*
Purpose: Take two points and calculate the cursor distance between them.
Original Offset: n/a
Return Value: Cursor distance
Status: Complete
*/
int __cdecl cursor_dist(int x_point_a, int y_point_a, int x_point_b, int y_point_b) {
    return (x_dist(x_point_a, x_point_b) + abs(y_point_a - y_point_b)) / 2;
}
