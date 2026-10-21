/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// Części wysięgników sieci trakcyjnej jako obiekty scenerii: odłączenie od pliku scenerii i usunięcie.
// Osobno od catenary_scene.cpp, żeby testy poza symulatorem mogły podstawić atrapę

#include "stdafx.h"

#include "model/AnimModel.h"
#include "scene/scenelayers.h"
#include "simulation/simulation.h"

// część należy do pliku sieci, nie do pliku scenerii, w którym akurat jest dyrektywa: bez warstwy zapis scenerii
// w edytorze jej nie przepisuje (inaczej każda część wysięgnika trafiłaby do scenerii jako zwykły model)
void
catenary_part_detach( TAnimModel *Model ) {

    if( Model == nullptr ) { return; }
    scene::Layers.count( Model->layer(), scene::layer_item::model, -1 );
    Model->layer( null_handle );
}

void
catenary_part_delete( TAnimModel *Model ) {

    if( Model == nullptr ) { return; }
    simulation::State.delete_model( Model );
}
