set(app_src DHT22.c MQ2.c)
set(pri_req)
idf_component_register(SRCS ${app_src}
                       INCLUDE_DIRS "include"
                       REQUIRES ${pri_req})