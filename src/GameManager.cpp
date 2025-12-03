/**
 * GameManager.cpp
 * Simple game manager for RTS game state and coordination.
 */

#include "GameManager.h"
#include "RTSCamera.h"
#include "SelectionManager.h"
#include "FlowFieldManager.h"
#include "UnitSpawner.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rts {

void GameManager::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("start_game"), &GameManager::start_game);
    ClassDB::bind_method(D_METHOD("pause_game"), &GameManager::pause_game);
    ClassDB::bind_method(D_METHOD("resume_game"), &GameManager::resume_game);
    ClassDB::bind_method(D_METHOD("end_game"), &GameManager::end_game);
    
    ClassDB::bind_method(D_METHOD("get_game_state"), &GameManager::get_game_state);
    ClassDB::bind_method(D_METHOD("get_game_time"), &GameManager::get_game_time);
    ClassDB::bind_method(D_METHOD("is_paused"), &GameManager::is_paused);
    
    // Signals
    ADD_SIGNAL(MethodInfo("game_started"));
    ADD_SIGNAL(MethodInfo("game_paused"));
    ADD_SIGNAL(MethodInfo("game_resumed"));
    ADD_SIGNAL(MethodInfo("game_ended"));
}

GameManager::GameManager() {
}

GameManager::~GameManager() {
}

void GameManager::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    find_components();
    setup_connections();
    
    // Start the game
    call_deferred("start_game");
}

void GameManager::_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    if (current_state == GameState::PLAYING && !paused) {
        game_time += delta;
    }
}

void GameManager::_input(const Ref<InputEvent> &event) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    
    // Handle pause with Escape key
    Input *input = Input::get_singleton();
    if (input->is_action_just_pressed("ui_cancel")) {
        if (paused) {
            resume_game();
        } else {
            pause_game();
        }
    }
}

void GameManager::start_game() {
    current_state = GameState::PLAYING;
    paused = false;
    game_time = 0.0f;
    
    UtilityFunctions::print("GameManager: Game started");
    emit_signal("game_started");
}

void GameManager::pause_game() {
    if (current_state != GameState::PLAYING) return;
    
    paused = true;
    current_state = GameState::PAUSED;
    
    // Optionally pause the scene tree
    // get_tree()->set_pause(true);
    
    UtilityFunctions::print("GameManager: Game paused");
    emit_signal("game_paused");
}

void GameManager::resume_game() {
    if (current_state != GameState::PAUSED) return;
    
    paused = false;
    current_state = GameState::PLAYING;
    
    // get_tree()->set_pause(false);
    
    UtilityFunctions::print("GameManager: Game resumed");
    emit_signal("game_resumed");
}

void GameManager::end_game() {
    current_state = GameState::GAME_OVER;
    
    UtilityFunctions::print("GameManager: Game ended");
    emit_signal("game_ended");
}

void GameManager::set_game_state(GameState state) {
    current_state = state;
}

int GameManager::get_game_state() const {
    return static_cast<int>(current_state);
}

void GameManager::find_components() {
    // Find camera
    Node *parent = get_parent();
    if (parent) {
        camera = Object::cast_to<RTSCamera>(parent->find_child("RTSCamera", true, false));
        selection_manager = Object::cast_to<SelectionManager>(parent->find_child("SelectionManager", true, false));
        flow_field_manager = Object::cast_to<FlowFieldManager>(parent->find_child("FlowFieldManager", true, false));
        unit_spawner = Object::cast_to<UnitSpawner>(parent->find_child("UnitSpawner", true, false));
    }
    
    // Log what was found
    if (camera) UtilityFunctions::print("GameManager: Found RTSCamera");
    if (selection_manager) UtilityFunctions::print("GameManager: Found SelectionManager");
    if (flow_field_manager) UtilityFunctions::print("GameManager: Found FlowFieldManager");
    if (unit_spawner) UtilityFunctions::print("GameManager: Found UnitSpawner");
}

void GameManager::setup_connections() {
    // Connect selection manager to flow field manager
    if (selection_manager && flow_field_manager) {
        selection_manager->set_flow_field_manager(flow_field_manager);
        UtilityFunctions::print("GameManager: Connected SelectionManager to FlowFieldManager");
    }
    
    // Connect unit spawner to selection manager
    if (unit_spawner && selection_manager) {
        unit_spawner->set_selection_manager(selection_manager);
        UtilityFunctions::print("GameManager: Connected UnitSpawner to SelectionManager");
    }
    
    // Connect unit spawner to flow field manager
    if (unit_spawner && flow_field_manager) {
        unit_spawner->set_flow_field_manager(flow_field_manager);
        UtilityFunctions::print("GameManager: Connected UnitSpawner to FlowFieldManager");
    }
    
    // Set camera for selection manager
    if (selection_manager && camera) {
        selection_manager->set_camera(camera);
        UtilityFunctions::print("GameManager: Connected SelectionManager to Camera");
    }
}

float GameManager::get_game_time() const {
    return game_time;
}

bool GameManager::is_paused() const {
    return paused;
}

RTSCamera* GameManager::get_camera() const {
    return camera;
}

SelectionManager* GameManager::get_selection_manager() const {
    return selection_manager;
}

FlowFieldManager* GameManager::get_flow_field_manager() const {
    return flow_field_manager;
}

UnitSpawner* GameManager::get_unit_spawner() const {
    return unit_spawner;
}

} // namespace rts
