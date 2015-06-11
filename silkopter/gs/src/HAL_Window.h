#pragma once

#include <QMainWindow>
#include <QtWidgets>

#include "Render_Widget.h"

#include "qneblock.h"
#include "qnodeseditor.h"
#include "qneport.h"

#include "HAL.h"
#include "Comms.h"

class QNodesEditor;
class JSON_Model;
class Sim_Window;

class HAL_Window : public QMainWindow
{
public:
    explicit HAL_Window(silk::HAL& hal, silk::Comms& comms, Render_Context& context, QWidget* parent = 0);
    ~HAL_Window();

    void process();

private:
    void on_node_factories_refreshed();
    void on_nodes_refreshed();
    void on_config_changed(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles);

private:
    void refresh_nodes();

    void contextMenu(QGraphicsSceneMouseEvent* event);
    void portContextMenu(QGraphicsSceneMouseEvent* event, QNEPort* port);
    void blockContextMenu(QGraphicsSceneMouseEvent* event, QNEBlock* block);
    void connectionContextMenu(QGraphicsSceneMouseEvent* event, QNEConnection* connection);

    void selection_changed();
    void set_config_editor_node(silk::node::Node_ptr node);

    void open_stream_viewer(std::string const& stream_name);

    void do_acceleration_calibration(silk::node::Node_ptr node, rapidjson::Value& biasj, rapidjson::Value& scalej);
    void do_magnetic_field_calibration(silk::node::Node_ptr node, rapidjson::Value& biasj);
    void do_angular_velocity_calibration(silk::node::Node_ptr node, rapidjson::Value& biasj);


    void refresh_node(silk::node::Node& node);
    void add_node(silk::node::Node_ptr node, QPointF pos);
    void create_node(silk::node::Node_Def_ptr def, QPointF pos);

    silk::HAL& m_hal;
    silk::Comms& m_comms;
    QNodesEditor* m_nodes_editor;
    QGraphicsView* m_view;
    QGraphicsScene* m_scene;

    struct Selection
    {
        silk::node::Node_ptr node;
        QDockWidget* config_dock = nullptr;
        QTreeView* config_view = nullptr;
        rapidjson::Document config_json;
        JSON_Model* config_model = nullptr;
    } m_selection;

    struct Stream_Data
    {
        silk::node::stream::Stream_wptr stream;
        std::shared_ptr<QNEPort> port;
        std::shared_ptr<QNEBlock> block;
    };

    struct Node_Data
    {
        silk::node::Node_wptr node;

        std::shared_ptr<QNEBlock> block;
        struct Input_Data
        {
            std::shared_ptr<QNEPort> port;
        };
        struct Output_Data
        {
            std::shared_ptr<QNEPort> port;
        };
        std::map<std::string, Input_Data> inputs;
        std::map<std::string, Output_Data> outputs;
    };

    std::map<std::string, Stream_Data> m_streams;
    std::map<std::string, Node_Data> m_nodes;

    Render_Context& m_context;
    Sim_Window* m_sim_window = nullptr;

    auto compute_unique_name(std::string const& name) const -> std::string;
};

