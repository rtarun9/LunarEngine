#include "LunarEngine/Engine.hpp"

int main()
{
    lunar::Engine engine{};

    try
    {
        engine.run();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[Exception Caught] : " << exception.what();
        return -1;
    }

    return 0;
}