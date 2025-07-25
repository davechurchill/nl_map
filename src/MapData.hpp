#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <iostream>
#include <bit>

#include <SFML/Graphics.hpp>


struct Node
{
    sf::Vector2f p;
    uint64_t id = 0;
    uint64_t index = 0;
    std::vector<uint64_t> wayIDs;
    std::vector<uint64_t> connectedNodeIDs;
    std::vector<uint64_t> connectedNodeIndexes;
    bool isPedestrian = false; 

    Node() = default;

    Node(float x, float y)
        : p(x, y)
    {
        id = pack_floats(x, y);
    }

    inline uint64_t pack_floats(float x, float y) 
    {
        return (uint64_t(std::bit_cast<uint32_t>(x)) << 32) | uint64_t(std::bit_cast<uint32_t>(y));
    }
};

struct Way 
{
    uint64_t index = 0;
    uint64_t id = 0;
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
    std::vector<Node> nodes;

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
            nodes.push_back(Node(y, -x));
        }

        id = std::stoull(way_id);

        return true;
    }

    bool isPedestrian() const 
    {
        return highway == "footway" || highway == "pedestrian"; 
    }
};

class NodeData
{
    std::unordered_map<uint64_t, Node> m_nodeMap;
    std::vector<Node> m_nodes;

public:

    NodeData() = default;

    void createVectorizedData()
    {
        std::cout << "Processing Node Data...";

        // create the vector with all the node data
        for (auto& [nodeID, node] : m_nodeMap)
        {
            node.index = m_nodes.size();
            m_nodes.push_back(node);
        }

        // loop over it one final time to get the node indexes
        for (auto& node : m_nodes)
        {
            for (size_t i = 0; i < node.connectedNodeIDs.size(); i++)
            {
                node.connectedNodeIndexes.push_back(getNodeByID(node.connectedNodeIDs[i]).index);
            }
        }
    }

    void addNode(const Node& node)
    {
        // check to see if the node's id is already in the map
        auto it = m_nodeMap.find(node.id);

        // if it's not in the map, add it to the map fresh
        if (it == m_nodeMap.end())
        {
            m_nodeMap.insert({ node.id, node });
        }
        // if it is in the map, add its connected node ids instead
        else
        {
            for (auto id : node.connectedNodeIDs)
            {
                it->second.connectedNodeIDs.push_back(id);
            }

            if (node.isPedestrian)
            {
                it->second.isPedestrian = true; 
            }
        }
    }

    std::vector<Node>& getNodes()
    {
        return m_nodes;
    }

    Node& getNodeByID(uint64_t id)
    {
        return m_nodeMap.at(id);
    }
};

class WayData
{
    std::unordered_map<uint64_t, Way> m_wayMap;

    std::vector<Way> m_ways;

public:

    WayData() = default;

    void addWay(Way& way)
    {
        m_wayMap.insert({ way.id, way });
    }

    std::vector<Way>& getWays()
    {
        return m_ways;
    }

    void createVectorizedData()
    {
        for (auto& [wayID, way] : m_wayMap)
        {
            way.index = m_ways.size();
            m_ways.push_back(way);

            if (way.isPedestrian())
            {
                for(auto& node : way.nodes)
                {
                    node.isPedestrian = true; 
                }
            }
        }
    }
};

class MapData 
{
    WayData     m_wayData;
    NodeData    m_nodeData;

public:

    void loadFromFile(const std::string& filename) 
    {
        std::ifstream file(filename);

        std::string line;

        // skip header
        if (!std::getline(file, line)) { return; }

        std::cout << "Loading Way Data from file...";
        while (std::getline(file, line)) 
        {
            Way way;
            if (way.parseFromCSVLine(line))
            {
                m_wayData.addWay(way);
            }
        }

        m_wayData.createVectorizedData();
        std::cout << " " << m_wayData.getWays().size() << " ways\n";
        
        for (auto& way : m_wayData.getWays())
        {
            for (size_t i = 0; i < way.nodes.size(); i++)
            {
                Node& n = way.nodes[i];
                n.isPedestrian = way.isPedestrian(); 

                // compute the connected nodes from this node
                if (i > 0) { n.connectedNodeIDs.push_back(way.nodes[i - 1].id); }
                if (i < way.nodes.size() - 1) { n.connectedNodeIDs.push_back(way.nodes[i + 1].id); }

                m_nodeData.addNode(n);
            }
        }

        m_nodeData.createVectorizedData();
        std::cout << " " << m_nodeData.getNodes().size() << " unique nodes\n";
    }

    std::vector<Way>& getWays()
    {
        return m_wayData.getWays();
    }

    std::vector<Node>& getNodes()
    {
        return m_nodeData.getNodes();
    }

    NodeData& getNodeData()
    {
        return m_nodeData;
    }
};
