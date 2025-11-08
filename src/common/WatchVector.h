#ifndef __AUTOMOTIVETDMANETWORK_WATCHVECTOR_H_
#define __AUTOMOTIVETDMANETWORK_WATCHVECTOR_H_

#include <iostream>
#include <vector>

// Overload of the << operator to allow WATCH_MAP to print std::vector<int>.
// This is placed in a separate header to be included where needed.
std::ostream& operator<<(std::ostream& os, const std::vector<int>& vec);

#endif
