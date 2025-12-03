/**
 * GameManager.h
 * Simple game manager for RTS game state and coordination.
 */

#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace rts {

class RTSCamera;
class SelectionManager;
class FlowFieldManager;
class UnitSpawner;

enum class GameState {
    INITIALIZING,
    PLAYING,
    PAUSED,
    GAME_OVER
};

class GameManager : public godot::Node {
    GDCLASS(GameManager, godot::Node)

private:
    // Game state
    GameState current_state = GameState::INITIALIZING;
    float game_time = 0.0f;
    
    // Component references
    RTSCamera *camera = nullptr;
    SelectionManager *selection_manager = nullptr;
    FlowFieldManager *flow_field_manager = nullptr;
    UnitSpawner *unit_spawner = nullptr;
    
    // Settings
    bool paused = false;

protected:
    static void _bind_methods();

public:
    GameManager();
    ~GameManager();

    void _ready() override;
    void _process(double delta) override;
    void _input(const godot::Ref<godot::InputEvent> &event) override;

    // Game state
    void start_game();
    void pause_game();
    void resume_game();
    void end_game();
    
    void set_game_state(GameState state);
    int get_game_state() const;
    
    // Component setup
    void find_components();
    void setup_connections();
    
    // Getters
    float get_game_time() const;
    bool is_paused() const;
    
    RTSCamera* get_camera() const;
    SelectionManager* get_selection_manager() const;
    FlowFieldManager* get_flow_field_manager() const;
    UnitSpawner* get_unit_spawner() const;
};

} // namespace rts

#endif // GAME_MANAGER_H
