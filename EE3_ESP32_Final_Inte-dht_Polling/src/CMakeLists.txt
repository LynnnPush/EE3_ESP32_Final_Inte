
file(GLOB_RECURSE COMPONENT_SRCS "*.c")

# Add include directory
set(COMPONENT_ADD_INCLUDEDIRS "../include")

# Register the component
register_component()
