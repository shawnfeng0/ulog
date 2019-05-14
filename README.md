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

    // Output level test
    Log_verbose("This is a test text.");
    Log_debug("This is a test text.");
    Log_info("This is a test text.");
    Log_warn("This is a test text.");
    Log_error("This is a test text.");
    Log_assert("This is a test text.\r\n");

    // Output color string test
    Log_info("text, " STR_BLACK "black, " STR_RESET "text");
    Log_info("text, " STR_RED "red, " STR_RESET "text");
    Log_info("text, " STR_GREEN "green, " STR_RESET "text");
    Log_info("text, " STR_YELLOW "yellow, " STR_RESET "text");
    Log_info("text, " STR_BLUE "blue, " STR_RESET "text");
    Log_info("text, " STR_PURPLE "purple, " STR_RESET "text");
    Log_info("text, " STR_SKYBLUE "skyblue, " STR_RESET "text");
    Log_info("text, " STR_WHITE "white, " STR_RESET "text\r\n");

    // Output token test
    Log_token(1+1);

    float pi = 3.14f;
    Log_token(50 / 180 * pi);

    return 0;
}
```

### Output

![ulog.png](https://i.postimg.cc/9FkJFb4r/ulog.png)
