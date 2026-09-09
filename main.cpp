#include "app/App.hpp"

/*
 * Everything - the oat++ Environment lifetime, config, the component graph, the
 * bind - lives in picasso::app. Keeping main() a one-liner means a test binary can
 * build the same graph without a process entry point in the way.
 */
int main() {
    return picasso::app::run();
}
