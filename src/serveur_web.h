// serveur_web.h — interface web (page unique), API fichiers et WebSocket d'état
//
// Les callbacks web tournent dans la tâche AsyncTCP (cœur 0) et ne font rien de long :
// les commandes WebSocket sont mises en file et exécutées par serveurweb::boucle() dans loop() (cœur 1).
#pragma once

namespace serveurweb {

void demarrer();
void boucle();

}  // namespace serveurweb
