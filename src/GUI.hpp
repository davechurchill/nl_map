#pragma once

#include "ViewController.hpp"
#include "MapData.hpp"
#include "EarCut.hpp"
#include "BoundryData.hpp"

#include <vector>
#include <map>
#include <memory>
#include <SFML/Graphics.hpp>

#include <queue>
#include <unordered_map> 
#include <unordered_set>
#include <vector> 
#include <cmath> 
#include <chrono>

#include "imgui.h"
#include "imgui-SFML.h"

class GUI
{
    sf::RenderWindow    m_window;
    sf::Clock           m_deltaClock;
    ImGuiStyle          m_originalStyle;
    ViewController      m_viewController;
    MapData             m_mapData;

    bool                m_drawWays = true;
    bool                m_drawNodes = false;
    int                 m_selectedNode = -1;

    int                 m_startNode = -1;
    int                 m_goalNode = -1;

    sf::VertexArray     m_wayLines{ sf::PrimitiveType::LineStrip };
    sf::VertexArray     m_nodeLines{ sf::PrimitiveType::Lines };

    sf::VertexArray     m_pathLines{sf::PrimitiveType::LineStrip};
    bool                m_pathFound = false; 
    bool                m_searchAttempted = false;

    // boundry data 
    BoundryDataSet      m_boundries;
    sf::VertexArray     m_boundryLines{ sf::PrimitiveType::LineStrip };
    std::vector<sf::VertexArray> m_landAreas;
    const sf::Color kLandColor{245, 161, 50}; 

    // Statistics for search:
    std::size_t  m_statsNodesSearched    = 0;    
    std::size_t  m_statsClosedListSize   = 0;    
    float        m_statsTotalDistance    = 0.f;  
    double       m_statsSearchTimeMs     = 0.0;  

    // Intermediate Nodes
    bool m_useIntermediateNodes = false;
    int m_intermediateNode1= -1; 
    int m_intermediateNode2= -1;

    struct TypeColor 
    {
        std::string type;
        ImVec4      color;  // RGBA in [0..1]
    };

    std::vector<TypeColor> m_colorOptions = 
    {
        { "footway",       ImVec4(0.f,   1.f,   0.f, 1.f) }, 
        { "residential",   ImVec4(1.f,   1.f,   0.f, 1.f) },
        { "motorway_link", ImVec4(1.f,   0.f,   1.f, 1.f) },
        { "primary",       ImVec4(1.f,   0.f,   0.f, 1.f) },
        { "secondary",     ImVec4(200.f / 255.f, 200.f / 255.f, 200.f / 255.f, 1.f) }, 
        { "tiertiary",     ImVec4(128.f / 255.f, 128.f / 255.f, 128.f / 255.f, 1.f) }  
    };

 struct searchNode{
            int nodeIndex; 
            int parentIndex; 
            float gCost; 
            float hCost; 
            
            float fCost() const 
            {
                return gCost + hCost;
            }

            // compare for priority queue min-heap based on fCost
            bool operator>(const searchNode& other) const {
                return fCost() > other.fCost();
            }
        }; 

public:
    
    GUI()
    {
        m_window.create(sf::VideoMode({ 1600, 900 }), "Grid View");
        m_window.setFramerateLimit(60);

        if (!ImGui::SFML::Init(m_window)) { exit(-1); }
        m_originalStyle = ImGui::GetStyle();

        m_mapData.loadFromFile("ways.txt");
        loadWayLines();
        loadWayLinesByNode();
        m_boundries.loadFromFile("boundries.txt");
        loadBoundryLines(); 
        buildLandAreas();
        setInitialView();
    }

    // sets the SFML window view to the minimum window that encapsulates all ways
    void setInitialView() 
    {
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();

        for (const auto& way : m_mapData.getWays()) 
        {
            for (const auto& n : way.nodes) 
            {
                if (n.p.x < minX) { minX = n.p.x; }
                if (n.p.y < minY) { minY = n.p.y; }
                if (n.p.x > maxX) maxX = n.p.x;
                if (n.p.y > maxY) maxY = n.p.y;
            }
        }

        if (minX == std::numeric_limits<float>::max()) { return; }

        float width = maxX - minX;
        float height = maxY - minY;
        sf::Vector2f center(minX + width / 2.f, minY + height / 2.f);
        sf::Vector2f size(width, height);

        sf::Vector2u winSize = m_window.getSize();
        float windowAspect = static_cast<float>(winSize.x) / winSize.y;
        float dataAspect = width / height;

        if (dataAspect > windowAspect) { size.y = width / windowAspect; }
        else { size.x = height * windowAspect; }

        sf::View view(center, size);
        m_window.setView(view);
    }

    void run()
    {
        while (true)
        {
            ImGui::SFML::Update(m_window, m_deltaClock.restart());
            m_window.clear();
            userInput();
            m_window.clear({173, 216, 230}); 
            render();
            imgui();
            ImGui::SFML::Render(m_window);
            m_window.display();
        }
    }

    void userInput()
    {
        while (auto event = m_window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(m_window, *event);
            m_viewController.processEvent(m_window, *event);

            if (event->is<sf::Event::Closed>())
            {
                std::exit(0);
            }

            if (ImGui::GetIO().WantCaptureMouse) { continue; }
            if (const auto* mbp = event->getIf<sf::Event::MouseButtonPressed>())
            {
                sf::Vector2f m(m_window.mapPixelToCoords(mbp->position));
                if (mbp->button == sf::Mouse::Button::Left)
                {
                    float minDist = 10000000;
                    int minIndex = -1;
                    for (Node& node : m_mapData.getNodes())
                    {
                        float dist = (node.p - m).length();
                        if (dist < minDist)
                        {
                            minDist = dist;
                            minIndex = int(node.index);
                        }
                    }
                    if ((m_pathFound || m_searchAttempted) && minIndex != m_startNode && minIndex != m_goalNode)
                    {
                        m_pathFound = false; //clear the previous path with
                        m_searchAttempted = false; 
                        m_pathLines.clear(); 

                        m_startNode = -1; 
                        m_goalNode = -1; 

                        m_intermediateNode1 = -1;
                        m_intermediateNode2 = -1;
                    }
                    m_selectedNode = minIndex;
                }
            }

        }
    }

    // gets a color from the colors list above based on a given key
    // sfml uses int, imgui uses float, so we need to convert
    sf::Color getColor(const std::string& key)
    {
        // find the key in the colors vector
        auto it = std::find_if(m_colorOptions.begin(), m_colorOptions.end(),
            [&](const TypeColor& tc) { return tc.type == key; } );

        // if it exists, use it
        if (it != m_colorOptions.end())
        {
            const ImVec4& c = it->color;
            return sf::Color(uint8_t(c.x * 255.0f), uint8_t(c.y * 255.0f), uint8_t(c.z * 255.0f), uint8_t(c.w * 255.0f));
        }

        // otherwise return white as default
        return sf::Color::White;
    }

    // load the way lines into a linestrip vertex buffer just once
    // the entire buffer will be drawn every frame by the gpu very quickly
    void loadWayLines()
    {
        std::cout << "Loading Way Lines into Vertex Array...\n";
        const std::vector<Way>& ways = m_mapData.getWays();

        for (size_t i=0; i<ways.size(); i++)
        {
            // in order to avoid connecting lines from the end of one way to the beginning of the next
            // we need to insert an invisible line in its place
            // this is the beginninf of that invisible line
            if (i > 0) { m_wayLines.append(sf::Vertex{ways[i].nodes[0].p, sf::Color(0, 0, 0, 0)}); }

            // now draw the actual line we want
            for (int c = 0; c < ways[i].count; c++)
            {
                sf::Color color = getColor(ways[i].highway);

                m_wayLines.append(sf::Vertex{ ways[i].nodes[c].p, color});
            }

            // then draw the end of the invisible line
            m_wayLines.append(sf::Vertex{ways[i].nodes[(ways[i].count-1)].p, sf::Color(0, 0, 0, 0) });
        }
    }

    void loadWayLinesByNode()
    {
        std::cout << "Loading Node Lines into Vertex Array...\n";
        const std::vector<Node>& nodes = m_mapData.getNodes();

        for (size_t i = 0; i < nodes.size(); i++)
        {
            for (auto id : nodes[i].connectedNodeIDs)
            {
                auto& node = m_mapData.getNodeData().getNodeByID(id);
                m_nodeLines.append(sf::Vertex{ nodes[i].p, sf::Color(255, 255, 255, 255) });
                m_nodeLines.append(sf::Vertex{ node.p, sf::Color(255, 255, 255, 255) });
            }
        }
    }
     // load the boundry lines into a linestrip vertex buffer just once
    // the entire buffer will be drawn every frame by the gpu very quickly
    void loadBoundryLines()
    {
        const std::vector<BoundryData>& boundries = m_boundries.getBoundries();

        for (const auto& boundry : boundries)
        {
            for (int c = 0; c < boundry.count; c++)
            {
                m_boundryLines.append(sf::Vertex{ boundry.nodes[c], sf::Color(179, 107, 0, 100) });
            }
            if (boundry.count > 0)
            {
                m_boundryLines.append(sf::Vertex{ boundry.nodes[0], sf::Color(179, 107, 0, 100) });
            }
        }
    }

    void buildLandAreas()
{
    using Coord = float;
    using Point = std::array<Coord, 2>;
    using Ring = std::vector<Point>;
    using Polygon = std::vector<Ring>;

    const auto& boundaries = m_boundries.getBoundries();
    if (boundaries.empty()) return;

    // Calculate the polygon area by using shoelace formula and it returns twice of the area (not including the 1/2 in formula)
    auto polygonArea = [](const std::vector<sf::Vector2f>& pts) 
    {
        float area = 0.f;
        for (size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) 
        {
            area += (pts[j].x + pts[i].x) * (pts[j].y - pts[i].y);
        }
        return area;
    };

    int outerIdx = 0;
    // Discard the sign bc physical size is important and not the orientation
    float maxArea = std::abs(polygonArea(boundaries[0].nodes));
    for (size_t i = 1; i < boundaries.size(); ++i) 
    {
        float area = std::abs(polygonArea(boundaries[i].nodes));
        if (area > maxArea) 
        {
            maxArea = area;
            outerIdx = i;
        }
    }

    // Build the polygon for earcut
    Polygon polygon;

    // Outer ring
    Ring outer;
    for (const auto& p : boundaries[outerIdx].nodes)
        outer.push_back({p.x, p.y});
    polygon.push_back(outer);

    // If want to add ponds so that it can handle holes inside the polygon
    // for (size_t i = 0; i < boundaries.size(); ++i) {
    //     if (i == outerIdx) continue;
    //     Ring hole;
    //     for (const auto& p : boundaries[i].nodes)
    //         hole.push_back({p.x, p.y});
    //     polygon.push_back(hole);
    // }

    // Triangulate
    std::vector<uint16_t> indices = mapbox::earcut<uint16_t>(polygon);

    // Build sf::VertexArray for drawing
    sf::VertexArray mesh(sf::PrimitiveType::Triangles);
    for (auto idx : indices) {
        // Find which ring this index belongs to
        size_t ringIdx = 0, ptIdx = idx;
        while (ringIdx < polygon.size() && ptIdx >= polygon[ringIdx].size()) 
        {
            ptIdx -= polygon[ringIdx].size();
            ++ringIdx;
        }
        if (ringIdx < polygon.size()) 
        {
            auto pt = polygon[ringIdx][ptIdx];
            mesh.append(sf::Vertex(sf::Vector2f(pt[0], pt[1]), kLandColor));
        }
    }

    m_landAreas.clear();
    if (mesh.getVertexCount() > 0)
        m_landAreas.push_back(std::move(mesh));
}


    void render()
    {

        //add the boundry lines and land 
        m_window.draw(m_boundryLines);

        for (const auto& landArea : m_landAreas)
        {
            m_window.draw(landArea);
        }

        if (m_drawWays) { m_window.draw(m_wayLines); }
        if (m_drawNodes) { m_window.draw(m_nodeLines); }

        if (m_selectedNode != -1)
        {
            float radius = 0.0002f;
            radius = m_window.getView().getSize().x / 100;
            
            for (uint64_t ni : m_mapData.getNodes()[m_selectedNode].connectedNodeIndexes)
            {
                drawCircleAtNode(int(ni), sf::Color(0, 255, 0, 200), radius);
            }
            drawCircleAtNode(m_selectedNode, sf::Color(255, 0, 0, 200), radius);
        }
        // add to draw circles on start and end node too
        if (m_startNode != -1)
        {
            float radius = 0.0002f;
            radius = m_window.getView().getSize().x / 100;
            
            for (uint64_t ni : m_mapData.getNodes()[m_startNode].connectedNodeIndexes)
            {
                drawCircleAtNode(int(ni), sf::Color(0, 255, 0, 200), radius);
            }
            drawCircleAtNode(m_startNode, sf::Color(255, 0, 0, 200), radius);

        }
        if (m_goalNode != -1)
        {
            float radius = 0.0002f;
            radius = m_window.getView().getSize().x / 100;
            
            for (uint64_t ni : m_mapData.getNodes()[m_goalNode].connectedNodeIndexes)
            {
                drawCircleAtNode(int(ni), sf::Color(0, 255, 0, 200), radius);
            }
            drawCircleAtNode(m_goalNode, sf::Color(255, 0, 0, 200), radius);

        }
        // draw node intermediate 1 
        if (m_useIntermediateNodes && m_intermediateNode1 != 1)
        { 
            float radius = 0.0002f;
            radius = m_window.getView().getSize().x / 100;
            
            for (uint64_t ni : m_mapData.getNodes()[m_intermediateNode1].connectedNodeIndexes)
            {
                drawCircleAtNode(int(ni), sf::Color(0, 255, 0, 200), radius);
            }
            drawCircleAtNode(m_intermediateNode1, sf::Color(255, 0, 0, 200), radius);
        }
        // draw node intermediate 2
        if (m_useIntermediateNodes && m_intermediateNode2 != -1)
        { 
            float radius = 0.0002f;
            radius = m_window.getView().getSize().x / 100;
            
            for (uint64_t ni : m_mapData.getNodes()[m_intermediateNode2].connectedNodeIndexes)
            {
                drawCircleAtNode(int(ni), sf::Color(0, 255, 0, 200), radius);
            }
            drawCircleAtNode(m_intermediateNode2, sf::Color(255, 0, 0, 200), radius);
        }
        // draw the path if it was found
        if (m_pathFound)
        {
            m_window.draw(m_pathLines);
        }
        
    }

    void drawCircleAtNode(int nodeIndex, sf::Color c, float radius)
    {
        sf::CircleShape circle(radius, 32);
        circle.setOrigin({ radius, radius });
        circle.setFillColor(c);
        circle.setPosition(m_mapData.getNodes()[nodeIndex].p);
        m_window.draw(circle);
    }

    void imgui()
    {
        ImGui::Begin("Map");

        if (ImGui::BeginTabBar("MyTabBar"))
        {
            if (ImGui::BeginTabItem("Way Info"))
            {
                ImGui::Text("Ways: %d", int(m_mapData.getWays().size()));
                ImGui::Text("Nodes: %d", int(m_mapData.getNodes().size()));
                if (ImGui::Button("Reset View"))
                {
                    setInitialView();
                }
                ImGui::Checkbox("Draw Ways", &m_drawWays);
                ImGui::Checkbox("Draw Nodes", &m_drawNodes);

                ImGui::Text("Selected Node ID: %d", m_selectedNode);
                ImGui::Text("   Start Node ID: %d", m_startNode);
                ImGui::Text("    Goal Node ID: %d", m_goalNode);
                if (ImGui::Button("Set Start")) { m_startNode = m_selectedNode; }
                ImGui::SameLine();
                if (ImGui::Button("Set Goal"))  { m_goalNode = m_selectedNode; }

                ImGui::Checkbox("Use Intermediate Nodes", &m_useIntermediateNodes);
                if (m_useIntermediateNodes)
                {
                    ImGui::Text("Intermediate Node 1 ID: %d", m_intermediateNode1);
                    ImGui::Text("Intermediate Node 2 ID: %d", m_intermediateNode2);
                    if (ImGui::Button("Set Intermediate Node 1")) { m_intermediateNode1 = m_selectedNode; }
                    ImGui::SameLine();
                    if (ImGui::Button("Set Intermediate Node 2")) { m_intermediateNode2 = m_selectedNode; }
                }
                if (ImGui::Button("Start Search"))
                {
                    m_searchAttempted = true;
                    m_pathLines.clear();

                    m_statsSearchTimeMs   = 0.0;
                    m_statsNodesSearched  = 0;
                    m_statsClosedListSize = 0;
                    m_statsTotalDistance  = 0.0f;

                    std::vector<int> fullPath;

                    if (m_useIntermediateNodes && m_intermediateNode1 != -1)
                    {
                        if (m_intermediateNode2 != -1)
                        {
                            auto path1 = dosearch(m_startNode, m_intermediateNode1);
                            auto path2 = dosearch(m_intermediateNode1, m_intermediateNode2);
                            auto path3 = dosearch(m_intermediateNode2, m_goalNode);

                            if (!path1.empty() && !path2.empty() && !path3.empty()) 
                            {
                                path1.pop_back(); 
                                path2.pop_back();

                                fullPath.insert(fullPath.end(), path1.begin(), path1.end());
                                fullPath.insert(fullPath.end(), path2.begin(), path2.end());
                                fullPath.insert(fullPath.end(), path3.begin(), path3.end());
                            }
                        }
                        else
                        {
                            auto path1 = dosearch(m_startNode, m_intermediateNode1);
                            auto path2 = dosearch(m_intermediateNode1, m_goalNode);

                            if (!path1.empty() && !path2.empty())
                            {
                                path1.pop_back();
                                fullPath.insert(fullPath.end(), path1.begin(), path1.end());
                                fullPath.insert(fullPath.end(), path2.begin(), path2.end());
                            }
                        }
                    }
                    else 
                    {
                        // direct path from start to goal
                        fullPath = dosearch(m_startNode, m_goalNode);
                    }

                    if (!fullPath.empty())
                    {
                        for (int idx : fullPath)
                            m_pathLines.append(sf::Vertex(m_mapData.getNodes()[idx].p, sf::Color::Blue));
                        m_pathFound = true;
                    }
                    else
                    {
                        m_pathFound = false;
                    }

                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Colors"))
            {
                for (auto& tc : m_colorOptions)
                {
                    if (ImGui::ColorEdit3(tc.type.c_str(), &tc.color.x))
                    {
                        m_wayLines.clear();
                        loadWayLines();
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Search Stats"))
            {
                ImGui::Text("Number of Nodes popped from open list: %zu", m_statsNodesSearched); 
                ImGui::Text("Closed List Size: %zu", m_statsClosedListSize);
                ImGui::Text("Total Distance Travelled km %.3f", m_statsTotalDistance);
                ImGui::Text("Search Time (ms): %.3f", m_statsSearchTimeMs);
                ImGui::EndTabItem(); 
            }
            ImGui::EndTabBar(); 
        }

        ImGui::End();
    }


    // heuristic function to estimate cost from current node to goal
    float estimateCost(const Node& a, const Node& b) 
    {
    sf::Vector2f diff = a.p - b.p;
    return std::sqrt(diff.x * diff.x + diff.y * diff.y);
    }

    void addToOpenList(std::priority_queue<searchNode, std::vector<searchNode>, std::greater<>>& openList, 
        int nodeIndex, int parentIndex, float gCost, float hCost)
    {
        openList.push(searchNode{nodeIndex, parentIndex, gCost, hCost});
    }
    void addToClosedList(std::unordered_set<int>& closedList, int nodeIndex)
    {
        closedList.insert(nodeIndex);
    }

    // define a priority for node expansion 
int getPriority(const Node& node)
{
    int priority = 50; // default worst-case

    for (auto wayID : node.wayIDs)
    {
        const Way& way = m_mapData.getWays()[wayID];

        // High-priority roads for buses
        if (way.highway == "motorway")            return 1;
        else if (way.highway == "trunk")          priority = std::min(priority, 2);
        else if (way.highway == "primary")        priority = std::min(priority, 3);

        // Mid-level bus-suitable roads
        else if (way.highway == "secondary")      priority = std::min(priority, 4);
        else if (way.highway == "tertiary")       priority = std::min(priority, 5);

        // Links (ramps, transitions)
        else if (way.highway == "motorway_link")  priority = std::min(priority, 6);
        else if (way.highway == "trunk_link")     priority = std::min(priority, 7);
        else if (way.highway == "primary_link")   priority = std::min(priority, 8);
        else if (way.highway == "secondary_link") priority = std::min(priority, 9);
        else if (way.highway == "tertiary_link")  priority = std::min(priority, 10);

        // Local/slow zones
        else if (way.highway == "residential")    priority = std::min(priority, 11);
        else if (way.highway == "living_street")  priority = std::min(priority, 12);

        else if (way.highway == "unclassified")   priority = std::min(priority, 13);
        else if (way.highway == "service")        priority = std::min(priority, 14);

        // Should be avoided for buses 
        else if (way.highway == "track" ||
                 way.highway == "path" ||
                 way.highway == "cycleway" ||
                 way.highway == "footway" ||
                 way.highway == "pedestrian" ||
                 way.highway == "steps" ||
                 way.highway == "corridor")
        {
            priority = std::min(priority, 100); // lowest priority = excluded
        }
    }

    return priority;
}

    // expand nodes based on only connected nodes are legal actions 
    void expandConnectedNodes(const std::vector<Node>& nodes, const searchNode& current, int goalNodeIndex, 
        std::unordered_set<int>& closedList, std::unordered_map<int, float>& gCosts, std::unordered_map<int, int>& parents, 
        std::priority_queue<searchNode, std::vector<searchNode>, std::greater<>>& openList)
    {
        const Node& currentNode = nodes[current.nodeIndex];

        for (int neighborIdx : currentNode.connectedNodeIndexes)
        {
            if(closedList.count(neighborIdx)) continue; 

            const Node& neighbor = nodes[neighborIdx];

            // Skip nodes that are part of ways with pedestrian highway tags 
            if (getPriority(neighbor) >= 100)
            {
                continue; 
            }

            float actionCost = estimateCost(currentNode, neighbor);

            // Penalize lower priority nodes slightly 
            actionCost *= getPriority(neighbor);

            float calculatedG = current.gCost + actionCost; 

            if (!gCosts.count(neighborIdx) || calculatedG < gCosts[neighborIdx])
            {
                gCosts[neighborIdx] = calculatedG; 
                float hCost = estimateCost(neighbor, nodes[goalNodeIndex]);
                addToOpenList(openList, neighborIdx, current.nodeIndex, calculatedG, hCost);

            }
        }
    }

    // Returns distance in meters between two (lon,lat) points on Earth.
    static double haversine(const sf::Vector2f& p0, const sf::Vector2f& p1)
    {
        static constexpr double R = 6'371'000.0; // meters
        double lat1 = p0.y * M_PI/180.0;
        double lon1 = p0.x * M_PI/180.0;
        double lat2 = p1.y * M_PI/180.0;
        double lon2 = p1.x * M_PI/180.0;

        double dlat = lat2 - lat1;
        double dlon = lon2 - lon1;
        double a = std::sin(dlat/2)*std::sin(dlat/2)
                + std::cos(lat1)*std::cos(lat2)
                * std::sin(dlon/2)*std::sin(dlon/2);
        double c = 2*std::atan2(std::sqrt(a), std::sqrt(1 - a));
        return R * c;
    }

std::vector<int> dosearch(int startNodeIndex, int goalNodeIndex)
{
    if (startNodeIndex == -1 || goalNodeIndex == -1)
        return {};

    const auto& nodes = m_mapData.getNodes();

    // start timer for this leg
    auto t0 = std::chrono::high_resolution_clock::now();

    std::priority_queue<searchNode, std::vector<searchNode>, std::greater<>> openList;
    std::unordered_set<int> closedSet;
    std::unordered_map<int, float> gCosts;
    std::unordered_map<int, int> parents;

    float h0 = estimateCost(nodes[startNodeIndex], nodes[goalNodeIndex]);
    addToOpenList(openList, startNodeIndex, -1, 0.f, h0);
    gCosts[startNodeIndex] = 0.f;

    bool   pathFound       = false;
    int    finalNodeIndex  = -1;
    size_t legNodesSearched = 0;

    while (!openList.empty())
    {
        auto current = openList.top();
        openList.pop();
        ++legNodesSearched;

        if (current.nodeIndex == goalNodeIndex)
        {
            pathFound      = true;
            finalNodeIndex = current.nodeIndex;
            parents[current.nodeIndex] = current.parentIndex;
            break;
        }

        if (closedSet.count(current.nodeIndex)) 
            continue;

        closedSet.insert(current.nodeIndex);
        parents[current.nodeIndex] = current.parentIndex;

        expandConnectedNodes(
            nodes, current, goalNodeIndex,
            closedSet, gCosts, parents,
            openList
        );
    }

    // stop timer and accumulate stats
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    m_statsSearchTimeMs   += elapsedMs;
    m_statsNodesSearched  += legNodesSearched;
    m_statsClosedListSize += closedSet.size();

    // rebuild path and accumulate distance
    std::vector<int> path;
    if (pathFound)
    {
        for (int at = finalNodeIndex; at != -1; at = parents[at])
            path.push_back(at);
        std::reverse(path.begin(), path.end());

        double legDistMeters = 0.0;
        for (size_t i = 1; i < path.size(); ++i)
        {
            legDistMeters += haversine(
                nodes[path[i-1]].p,
                nodes[path[i  ]].p
            );
        }
        m_statsTotalDistance += static_cast<float>(legDistMeters / 1000.0f); //convert to km

        // output the path 
        std::cout << "Path found! Nodes visited: " << path.size() << "\n"; 
        for (int idx: path) 
        {
            const auto& p = nodes[idx].p; 
            std::cout << " Node Index: " << idx << " (Position: " << p.x << ", " << p.y << ")\n";
        }
    }

    else{
        std::cout << "No path found from " << startNodeIndex << " to " << goalNodeIndex << "\n";
    }

    return path;
}


};
