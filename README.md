# ulog

Ulog is a micro log library suitable for use with lightweight embedded devices.

## Example

### Code

```C++
#include <stdio.h>
#include "ulog/ulog.h"

int put_str(const char *str) {
    return printf("%s", str);
}

int main() {

    uLogInit(put_str);

    // Different log levels
    Log_verbose("This is a test text.");
    Log_debug("This is a test text.");
    Log_info("This is a test text.");
    Log_warn("This is a test text.");
    Log_error("This is a test text.");
    Log_assert("This is a test text.\r\n");

    // Output different colored strings
    Log_info(STR_RESET "text, " STR_BLACK "black, " STR_RESET "text");
    Log_info(STR_RESET "text, " STR_RED "red, " STR_RESET "text");
    Log_info(STR_RESET "text, " STR_GREEN "green, " STR_RESET "text");
    Log_info(STR_RESET "text, " STR_YELLOW "yellow, " STR_RESET "text");
    Log_info(STR_RESET "text, " STR_BLUE "blue, " STR_RESET "text");
    Log_info(STR_RESET "text, " STR_PURPLE "purple, " STR_RESET "text");
    Log_info(STR_RESET "text, " STR_SKYBLUE "skyblue, " STR_RESET "text");
    Log_info(STR_RESET "text, " STR_WHITE "white, " STR_RESET "text\r\n");

    // Output debugging expression
    double pi = 3.14;
    Log_token(&pi); // print address of pi
    Log_token(pi);
    Log_token(pi*50.f/180.f);
    return 0;
}
```

### Output

[![2019-06-06-01-25.png](https://i.postimg.cc/JhNkgZ8r/2019-06-06-01-25.png)](https://postimg.cc/bGvrSGdW)
