find_package(Doxygen REQUIRED) 

# Поиск всех необходимых заголовков самого приложения
get_target_property(HEADER_DIRS hkreduce INCLUDE_DIRS)
file(GLOB_RECURSE HEADER_DIRS ${HEADER_DIRS}/hkreduce/*.h)

# Указываем input для Doxygen
set(DOXYGEN_INPUT_DIR ${SRC_DIRS}/hkreduce)
# Основная папка для вывода Doxygen
set(DOXYGEN_OUTPUT_DIR ${OUTPUT_DIR}/docs/doxygen)
# Куда положить сгенерированный "сайт" с документацией
set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/html/index.html)
# Шаблон конфиг файла Doxygen
set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
# Путь до итогого форматированого конфиг файла Doxygen
set(DOXYFILE_OUT ${DOXYGEN_OUTPUT_DIR}/Doxyfile)

# Форматирование шаблона конфиг файла Doxygen
configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY) 

# Создаем папку для вывода
file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR})

# Команда для генерации документации
add_custom_command(
    OUTPUT ${DOXYGEN_INDEX_FILE}
    DEPENDS ${HEADER_DIRS}
    COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    MAIN_DEPENDENCY Doxyfile
    COMMENT "Generating docs"
)

# Добавлении "пустой" цели к процессу сборки с зависимостью в виде документации, которая генерируется командой выше.
# Это вызовет генерацию документации во время сборки
add_custom_target(doxygen ALL DEPENDS ${DOXYGEN_INDEX_FILE})