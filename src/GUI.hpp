#pragma once

#include "ViewController.hpp"
#include "WayData.hpp"

#include <vector>
#include <map>
#include <memory>
#include <SFML/Graphics.hpp>

#include "imgui.h"
#include "imgui-SFML.h"

class GUI
{
    sf::RenderWindow    m_window;
    sf::Clock           m_deltaClock;
    ImGuiStyle          m_originalStyle;
    ViewController      m_viewController;
    WayDataSet          m_ways;

    sf::VertexArray     m_wayLines{ sf::PrimitiveType::LineStrip };

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

public:
    
    GUI()
    {
        m_window.create(sf::VideoMode({ 1600, 900 }), "Grid View");
        m_window.setFramerateLimit(60);

        if (!ImGui::SFML::Init(m_window)) { exit(-1); }
        m_originalStyle = ImGui::GetStyle();

        m_ways.loadFromFile("ways.txt");
        loadWayLines();
        setInitialView();
    }

    // sets the SFML window view to the minimum window that encapsulates all ways
    void setInitialView() 
    {
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();

        for (const auto& way : m_ways.getWays()) 
        {
            for (const auto& p : way.nodes) 
            {
                if (p.x < minX) { minX = p.x; }
                if (p.y < minY) { minY = p.y; }
                if (p.x > maxX) maxX = p.x;
                if (p.y > maxY) maxY = p.y;
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
        const std::vector<WayData>& ways = m_ways.getWays();

        for (size_t i=0; i<ways.size(); i++)
        {
            // in order to avoid connecting lines from the end of one way to the beginning of the next
            // we need to insert an invisible line in its place
            // this is the beginninf of that invisible line
            if (i > 0) { m_wayLines.append(sf::Vertex{ways[i].nodes[0], sf::Color(0, 0, 0, 0)}); }

            // now draw the actual line we want
            for (int c = 0; c < ways[i].count; c++)
            {
                sf::Color color = getColor(ways[i].highway);

                m_wayLines.append(sf::Vertex{ ways[i].nodes[c], color});
            }

            // then draw the end of the invisible line
            m_wayLines.append(sf::Vertex{ways[i].nodes[(ways[i].count-1)], sf::Color(0, 0, 0, 0) });
        }
    }

    void render()
    {
        m_window.draw(m_wayLines);
    }

    void imgui()
    {
        ImGui::Begin("Map");

        if (ImGui::BeginTabBar("MyTabBar"))
        {
            if (ImGui::BeginTabItem("Way Info"))
            {
                ImGui::Text("Ways: %d", int(m_ways.getWays().size()));
                if (ImGui::Button("Reset View"))
                {
                    setInitialView();
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

            ImGui::EndTabBar();
        }

        ImGui::End();
    }
};