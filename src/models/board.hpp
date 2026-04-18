#pragma once
#include <vector>
#include "hex.hpp"


class Board {
public:
    static constexpr int WIDTH = 15;
    static constexpr int HEIGHT = 11;

    Board() {
        grid.reserve(WIDTH * HEIGHT);

        for (int row = 0; row < HEIGHT; ++row) {
            for (int col = 0; col < WIDTH; ++col) {
                int q = col - (row - (row & 1)) / 2;
                int r = row;
                int s = -q - r;

                grid.emplace_back(q, r, s);
            }
        }
    }

    Hex& get_hex_at_offset(int col, int row) {
        return grid[row * WIDTH + col];
    }

    Hex& get_hex(int q, int r, int s) {
        for (Hex& hex : grid) {
            if (hex.get_q() == q && hex.get_r() == r && hex.get_s() == s) {
                return hex;
            }
        }
        throw std::out_of_range("Hex with given coordinates not found");
    }

private:
    std::vector<Hex> grid;
};