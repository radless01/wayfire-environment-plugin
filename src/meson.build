shared_module(
    'environment',
    sources: files(
        'environment.cpp',
    ),

    dependencies: [
        wayfire
    ],

    install: true,
    install_dir: wayfire.get_variable(pkgconfig: 'plugindir'),

    name_prefix: 'lib',

    cpp_args: [
        '-D_FILE_OFFSET_BITS=64',
    ],
)