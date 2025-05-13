#include "echo.hpp"

int main()
{
    EchoServer echo(8080);
    echo.Start();
    return 0;
}