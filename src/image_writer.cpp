#include "arena.h"
#include "image_util.h"
#include "image_writer.h"

#include <fstream>

using namespace wotreplay;

const int element_size = 48;

void image_writer_t::draw_element(const boost::multi_array<uint8_t, 3> &element, int x, int y, int mask) {
    const size_t *shape = element.shape();
    const size_t *image_size = base.shape();
    for (int i = 0; i < shape[0]; ++i) {
        for (int j = 0; j < shape[1]; ++j) {
            // don't touch alpha channel, use from original
            for (int k = 0; k < 3; ++k) {
                if (((i + y) >= image_size[0]) || ((j + x) >= image_size[1])) {
                    continue;
                }
                base[i + y][j + x][k] = mix(base[i + y][j + x][k], base[i + y][j + x][k], (255 - element[i][j][3]) / 255.f,
                                    element[i][j][k] & ((mask >> ((4- (k + 1))*8)) & 0xFF), element[i][j][3] / 255.f);
            }
        }
    }
}

boost::multi_array<uint8_t, 3> image_writer_t::get_element(const std::string &name) {
    boost::multi_array<uint8_t, 3> element, resized_element;
    std::ifstream is("elements/" + name + ".png", std::ios::binary);
    read_png(is, element);
    resize(element, element_size, element_size, resized_element);
    return resized_element;
}

void image_writer_t::draw_element(const boost::multi_array<uint8_t, 3> &element, std::tuple<float, float> position, int mask) {
    auto shape = base.shape();
    auto element_shape = element.shape();
    float x,y;
    auto position3d = std::make_tuple(std::get<0>(position), 0.f, std::get<1>(position));
    std::tie(x,y) = get_2d_coord(position3d, arena.bounding_box, (int) shape[1], (int) shape[0]);
    draw_element(element, x - element_shape[1]/2, y - element_shape[0]/2, mask);
}

void image_writer_t::draw_grid(boost::multi_array<uint8_t, 3> &image) {
    const int grid_line_width = 2;
    const int grid_cells = 10;
    auto shape = image.shape();
    int grid_width = ((int) shape[1] - (grid_cells + 1) * grid_line_width) / grid_cells;
    int grid_height = ((int) shape[0] - (grid_cells + 1) * grid_line_width) / grid_cells;
    for (int y = 0; y < shape[0]; y += (grid_height + grid_line_width)) {
        for (int i = 0; i < grid_line_width; ++i) {
            for (int x = 0; x < shape[1]; ++x) {
                for (int c = 0; c < 3; ++ c) {
                    image[y + i][x][c] = std::min(image[y][x][c] + 20, 255);
                }
            }
        }
    }
    for (int x = 0; x < shape[1]; x += (grid_width + grid_line_width)) {
        for (int i = 0; i < grid_line_width; ++i) {
            for (int y = 0; y < shape[0]; ++y) {
                for (int c = 0; c < 3; ++ c) {
                    image[y][x + i][c] = std::min(image[y][x][c] + 20, 255);
                }
            }
        }
    }
}

void image_writer_t::draw_elements() {
    const arena_configuration_t &configuration = arena.configurations[mode];

    int reference_team_id = use_fixed_teamcolors ? 0 : recorder_team;
    
    if (mode == "dom") {
        auto neutral_base = get_element("neutral_base");
        draw_element(neutral_base, configuration.control_point);
    }

    auto friendly_base = get_element("friendly_base");
    auto enemy_base = get_element("enemy_base");
    for(const auto &entry : configuration.team_base_positions) {
        for (const auto &position : entry.second) {
            draw_element((entry.first - 1) == reference_team_id ? friendly_base : enemy_base, position);
        }
    }

    std::vector<boost::multi_array<uint8_t, 3>> spawns{
        get_element("neutral_spawn1"),
        get_element("neutral_spawn2"),
        get_element("neutral_spawn3"),
        get_element("neutral_spawn4")
    };
    
    for(const auto &entry : configuration.team_spawn_points) {
        for (int i = 0; i < entry.second.size(); ++i) {
            int mask = (reference_team_id == (entry.first - 1)) ? 0x00FF00FF : 0xFF0000FF;
            draw_element(spawns[i], entry.second[i], mask);
        }
    }

    draw_grid(base);
}

void image_writer_t::draw_death(const packet_t &packet, const game_t &game) {
    uint32_t killer, killed;
    std::tie(killed, killer) = packet.tank_destroyed();
    packet_t position_packet;
    bool found = game.find_property(packet.clock(), killed, property_t::position, position_packet);
    if (found) {
        draw_position(position_packet, game, this->deaths);
    }
}

void image_writer_t::draw_position(const packet_t &packet, const game_t &game, boost::multi_array<float, 3> &image) {
    uint32_t player_id = packet.player_id();

    int team_id = game.get_team_id(player_id);
    if (team_id < 0) return;

    auto shape = image.shape();
    int height = static_cast<int>(shape[1]);
    int width = static_cast<int>(shape[2]);

    const bounding_box_t &bounding_box = game.get_arena().bounding_box;
    std::tuple<float, float> position = get_2d_coord( packet.position(), bounding_box, width, height);
    long x = std::lround(std::get<0>(position));
    long y = std::lround(std::get<1>(position));
    if (x >= 0 && y >= 0 && x < width && y < height) {
        image[team_id][y][x]++;
        if (player_id == game.get_recorder_id()) {
            image[2][y][x]++;
        }
    }
}

void image_writer_t::update(const game_t &game) {
    recorder_team = game.get_team_id(game.get_recorder_id());
    
    std::set<int> dead_players;
    for (const packet_t &packet : game.get_packets()) {
        if (packet.has_property(property_t::position)
            && dead_players.find(packet.player_id()) == dead_players.end()) {
            draw_position(packet, game, this->positions);
        } else if (packet.has_property(property_t::tank_destroyed)) {
            uint32_t target, killer;
            std::tie(target, killer) = packet.tank_destroyed();
            dead_players.insert(target);
            draw_death(packet, game);
        }
    }
}

void image_writer_t::load_base_map(const std::string &path) {
    std::ifstream is(path, std::ios::binary);
    read_png(is, this->base);
}

void image_writer_t::write(std::ostream &os) {
    write_png(os, result);
}

void image_writer_t::init(const arena_t &arena, const std::string &mode) {
    this->arena = arena;
    this->mode = mode;
    load_base_map(arena.mini_map);
    const size_t *shape = base.shape();
    size_t height = shape[0], width = shape[1];
    positions.resize(boost::extents[3][height][width]);
    deaths.resize(boost::extents[3][height][width]);
    clear();
    initialized = true;
}

void image_writer_t::clear() {
    std::fill(result.origin(), result.origin() + result.num_elements(), 0.f);
    std::fill(deaths.origin(), deaths.origin() + deaths.num_elements(), 0.f);
    std::fill(positions.origin(), positions.origin() + positions.num_elements(), 0.f);
}

void image_writer_t::finish() {
    // copy background to result
    const size_t *shape = base.shape();
    result.resize(boost::extents[shape[0]][shape[1]][shape[2]]);
    draw_elements();
    result = base;
    int reference_team_id = use_fixed_teamcolors ? 0 : recorder_team;
    for (int i = 0; i < shape[0]; ++i) {
        for (int j = 0; j < shape[1]; ++j) {
            if (positions[0][i][j] > positions[1][i][j]) {
                // position claimed by first team
                if (reference_team_id == 0) {
                    result[i][j][0] = result[i][j][2] = 0x00;
                    result[i][j][1] = result[i][j][3] = 0xFF;
                } else {
                    result[i][j][1] = result[i][j][2] = 0x00;
                    result[i][j][0] = result[i][j][3] = 0xFF;
                }
                
            } else if (positions[0][i][j] < positions[1][i][j]) {
                // position claimed by second team
                if (reference_team_id == 0) {
                    result[i][j][1] = result[i][j][2] = 0x00;
                    result[i][j][0] = result[i][j][3] = 0xFF;
                } else {
                    result[i][j][0] = result[i][j][2] = 0x00;
                    result[i][j][1] = result[i][j][3] = 0xFF;
                }
            } else {
                // no change
            }

            if (this->show_self && positions[2][i][j] > 0.f) {
                // position claimed by second team
                result[i][j][0] = result[i][j][1] = 0x00;
                result[i][j][2] = result[i][j][3] = 0xFF;
            }

            if (deaths[0][i][j] + deaths[1][i][j] > 0.f) {
                static int offsets[][2] = {
                    {-1, -1},
                    {-1,  0},
                    {-1,  1},
                    { 0,  1},
                    { 1,  1},
                    { 1,  0},
                    { 1, -1},
                    { 0, -1},
                    { 0,  0}
                };

                for (const auto &offset : offsets) {
                    int x = j + offset[0];
                    int y = i + offset[1];

                    // draw only if within bounds
                    if (x >= 0 && y < shape[0] && y >= 0  && y < shape[1]) {
                        result[y][x][3] = result[y][x][0] = result[y][x][1] = 0xFF;
                        result[y][x][2] = 0x00;
                    }
                }
            }
        }
    }
}

void image_writer_t::reset() {
    // initialize with empty image arrays
    initialized = false;
}

bool image_writer_t::is_initialized() const {
    return initialized;
}

void image_writer_t::set_show_self(bool show_self) {
    this->show_self = show_self;
}

bool image_writer_t::get_show_self() const {
    return show_self;
}

void image_writer_t::set_use_fixed_teamcolors(bool use_fixed_teamcolors) {
    this->use_fixed_teamcolors = use_fixed_teamcolors;
}

bool image_writer_t::get_use_fixed_teamcolors() const {
    return use_fixed_teamcolors;
}

void image_writer_t::set_recorder_team(int recorder_team) {
    this->recorder_team = recorder_team;
}

int image_writer_t::get_recorder_team() const {
    return recorder_team;
}
