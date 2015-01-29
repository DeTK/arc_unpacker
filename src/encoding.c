#include <errno.h>
#include <iconv.h>
#include <stdlib.h>
#include "assert.h"
#include "logger.h"

int convert(
    const char *input,
    size_t input_size,
    char **output,
    size_t *output_size,
    const char *const from,
    const char *const to)
{
    assert_not_null(input);
    assert_not_null(output);
    assert_not_null(output_size);
    assert_not_null(from);
    assert_not_null(to);

    iconv_t conv = iconv_open(to, from);

    *output = NULL;
    *output_size = 0;

    char *output_old, *output_new;
    char *input_ptr = (char*) input;
    char *output_ptr = NULL;
    size_t input_bytes_left = input_size;
    size_t output_bytes_left = *output_size;

    while (1)
    {
        output_old = *output;
        output_new = realloc(*output, *output_size);
        if (!output_new)
        {
            log_error(NULL);
            free(*output);
            *output = NULL;
            return false;
        }
        *output = output_new;

        if (output_old == NULL)
            output_ptr = *output;
        else
            output_ptr += *output - output_old;

        int result = iconv(
            conv,
            &input_ptr,
            &input_bytes_left,
            &output_ptr,
            &output_bytes_left);

        if (result != -1)
            break;

        switch (errno)
        {
            case EINVAL:
            case EILSEQ:
                free(*output);
                *output = NULL;
                *output_size = 0;
                return errno;

            case E2BIG:
                *output_size += 1;
                output_bytes_left += 1;
                continue;
        }
    }

    iconv_close(conv);
    return true;
}
