# -*- cmake -*-
# Copyright (C) Dmitry Igrishin
# For conditions of distribution and use, see files LICENSE.txt

if (NOT ("${PG_SHAREDIR}" STREQUAL ""))
  install(FILES
    dmitigr_spa--0.1.sql
    dmitigr_spa.control

    DESTINATION "${PG_SHAREDIR}/extension")
endif()
