// atrapa world/catenary_scene_objects.cpp: liczy usunięte części
#include "model/AnimModel.h"
int mock_deleted_parts { 0 };
void catenary_part_detach( TAnimModel * ) {}
void catenary_part_delete( TAnimModel * ) { ++mock_deleted_parts; }
