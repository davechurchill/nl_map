#pragma once 

#include <string> 
#include <sstream>
#include <vector> 
#include <fstream> 
#include <iostream> 

#include <SFML/Graphics.hpp>

struct BoundryData
{
    std::string nodes_count; 
    int count = 0;
    std::vector<sf::Vector2f> nodes;

    bool parseFromCSVLine(const std::string& line)
    {
        std::stringstream ss(line); 
        std::string token; 

        #define READ(f) { if (!std::getline(ss, f, ',')) { return false; } }

        READ(nodes_count); 

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

class BoundryDataSet
{
    std::vector<BoundryData> m_boundries;

public:

    bool loadFromFile(const std::string& filename)
    {
        std::ifstream file(filename);

        std::string line; 

        //skip header
        if (!std::getline(file, line)) { return false; }
        
        while (std::getline(file, line))
        {
            BoundryData boundry; 
            if (boundry.parseFromCSVLine(line))
            {
                m_boundries.push_back(std::move(boundry));
            }
        }

        std::cout << "Loaded " << m_boundries.size() << " boundries\n"; 

        return true; 
    }

    const std::vector<BoundryData>& getBoundries() const
    {
        return m_boundries;
    }

}; 