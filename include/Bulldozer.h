/**
 * Bulldozer.h
 * Construction vehicle that can build structures.
 * Inspired by C&C Generals Dozer.
 */

#ifndef BULLDOZER_H
#define BULLDOZER_H

#include "Vehicle.h"
#include "Building.h"

namespace rts {

enum class BuildingType {
    NONE,
    POWER_PLANT,
    BARRACKS
};

class Bulldozer : public Vehicle {
    GDCLASS(Bulldozer, Vehicle)

private:
    // Construction state
    bool is_constructing = false;
    float construction_progress = 0.0f;
    float construction_time = 5.0f;  // Seconds to build
    BuildingType current_build_type = BuildingType::NONE;
    godot::Vector3 build_location;
    
    // Ghost preview
    godot::Node3D *ghost_building = nullptr;
    bool is_placing_building = false;
    BuildingType placing_type = BuildingType::NONE;
    
    // Building costs (for future resource system)
    int power_plant_cost = 500;
    int barracks_cost = 800;
    
    // Model paths for buildings
    godot::String power_plant_model = "res://assets/buildings/power/power.glb";
    godot::String barracks_model = "";  // Empty = use default box

protected:
    static void _bind_methods();

public:
    Bulldozer();
    ~Bulldozer();

    void _ready() override;
    void _process(double delta) override;
    void _physics_process(double delta) override;
    
    void create_vehicle_mesh() override;
    
    // Building commands
    void start_placing_building(int type);  // 0=PowerPlant, 1=Barracks
    void cancel_placing();
    void confirm_build_location(const godot::Vector3 &location);
    void update_ghost_position(const godot::Vector3 &position);
    
    // Construction
    void start_construction();
    void update_construction(double delta);
    void complete_construction();
    void cancel_construction();
    
    bool get_is_constructing() const;
    float get_construction_progress() const;
    bool get_is_placing_building() const;
    int get_placing_type() const;
    
    // Getters for costs
    int get_power_plant_cost() const;
    int get_barracks_cost() const;
    
    void set_power_plant_model(const godot::String &path);
    godot::String get_power_plant_model() const;
    
    void set_barracks_model(const godot::String &path);
    godot::String get_barracks_model() const;

private:
    void create_ghost_building(BuildingType type);
    void remove_ghost_building();
    Building* spawn_building(BuildingType type, const godot::Vector3 &position);
};

} // namespace rts

#endif // BULLDOZER_H
