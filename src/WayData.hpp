#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <iostream>

#include <SFML/Graphics.hpp>

struct WayData 
{
    std::string way_id;
    std::string highway;
    std::string name;
    std::string type;
    std::string service;
    std::string maxspeed;
    std::string oneway;
    std::string bicycle;
    std::string foot;
    std::string access;
    std::string sidewalk;
    std::string surface;
    std::string lanes;
    std::string lit;
    std::string bridge;
    std::string tunnel;
    std::string motor_vehicle;
    std::string motorcar;
    std::string bus;
    std::string area;
    std::string junction;
    std::string nodes_count;
    int         count = 0;
    std::vector<sf::Vector2f> nodes;

    bool parseFromCSVLine(const std::string& line) 
    {
        std::stringstream ss(line);
        std::string token;

        #define READ(f) { if (!std::getline(ss, f, ',')) { return false; } }

        READ(way_id);   READ(highway);       READ(name);     READ(type); READ(service);
        READ(maxspeed); READ(oneway);        READ(bicycle);  READ(foot); READ(access);
        READ(sidewalk); READ(surface);       READ(lanes);    READ(lit);  READ(bridge);
        READ(tunnel);   READ(motor_vehicle); READ(motorcar); READ(bus);  READ(area);
        READ(junction); READ(nodes_count);

        try { count = std::stoi(nodes_count); }
        catch (...) { return false; }

        float x = 0, y = 0;
        for (int i = 0; i < 2 * count; ++i)
        {
            if (!std::getline(ss, token, ',')) break;
            x = std::stof(token);
            if (!std::getline(ss, token, ',')) break;
            y = std::stof(token);

            // the data needed to be rotated to match the sfml drawing direction
            nodes.push_back({y, -x});
        }

        return true;
    }
};

class WayDataSet 
{
    std::vector<WayData> m_ways;

public:

    bool loadFromFile(const std::string& filename) 
    {
        std::ifstream file(filename);

        std::string line;

        // skip header
        if (!std::getline(file, line)) { return false; }

        while (std::getline(file, line)) 
        {
            WayData way;
            if (way.parseFromCSVLine(line))
            {
                m_ways.push_back(std::move(way));
            }
        }

        std::cout << "Loaded " << m_ways.size() << " ways\n";

        return true;
    }

    const std::vector<WayData>& getWays() const
    {
        return m_ways;
    }
};
