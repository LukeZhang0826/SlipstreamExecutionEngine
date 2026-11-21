

#include "slipstream/config.h"
#include "slipstream/engine.h"

int main(int argc, char** argv) {
    Config cfg;
    if (!parse_args(argc, argv, cfg)) return 2;

    Engine engine{cfg};
    if (!engine.bind()) return 1;

    engine.run();
    engine.report();
    return 0;
}
