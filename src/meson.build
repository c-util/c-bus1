#
# target: libcbus1.so
#

libcbus1_symfile = join_paths(meson.current_source_dir(), 'libcbus1.sym')

libcbus1_deps = [
        dep_clist,
        dep_crbtree,
        dep_cstdaux,
]

libcbus1_both = both_libraries(
        'cbus1-'+major,
        [
                'c-bus1.c',
                'c-bus1-sys.c',
        ],
        c_args: [
                '-fvisibility=hidden',
                '-fno-common',
        ],
        dependencies: libcbus1_deps,
        install: not meson.is_subproject(),
        link_args: dep_cstdaux.get_variable('version-scripts') == 'yes' ? [
                '-Wl,--version-script=@0@'.format(libcbus1_symfile),
        ] : [],
        link_depends: libcbus1_symfile,
        soversion: 0,
)

libcbus1_dep = declare_dependency(
        dependencies: libcbus1_deps,
        include_directories: include_directories('.'),
        link_with: libcbus1_both.get_static_lib(),
        version: meson.project_version(),
)

if not meson.is_subproject()
        install_headers('c-bus1.h')

        mod_pkgconfig.generate(
                description: project_description,
                filebase: 'libcbus1-'+major,
                libraries: libcbus1_both.get_shared_lib(),
                name: 'libcbus1',
                version: meson.project_version(),
        )
endif

#
# target: test-*
#

test_api = executable('test-api', ['test-api.c'], link_with: libcbus1_both.get_shared_lib())
test('API Symbol Visibility', test_api)

test_basic = executable('test-basic', ['test-basic.c'], dependencies: libcbus1_dep)
test('Basic API Behavior', test_basic)
