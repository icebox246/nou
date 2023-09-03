#ifndef DA_H_
#define DA_H_

#define da_append(arr, it)                                                   \
    do {                                                                     \
        if ((arr).capacity <= (arr).count) {                                 \
            if ((arr).capacity == 0) (arr).capacity = 2;                     \
            (arr).items = realloc(                                           \
                (arr).items, sizeof((arr).items[0]) * ((arr).capacity * 2)); \
            (arr).capacity *= 2;                                             \
        }                                                                    \
        (arr).items[(arr).count++] = (it);                                   \
    } while (0)

#define da_list(t) \
    t* items;      \
    size_t count;  \
    size_t capacity

#endif
