#
# target: libcbus1.so
#

libcbus1_symfile = join_paths(meson.current_source_dir(), 'libcbus1.sym')

libcbus1_deps = [
        dep_clist,
        dep_crbtree,
        dep_cstdaux,
]

libcbus1_private = static_library(
        'cbus1-private',
        [
                'c-bus1.c',
                'c-bus1-sys.c',
        ],
        c_args: [
                '-fvisibility=hidden',
                '-fno-common',
        ],
        dependencies: libcbus1_deps,
        pic: true,
)

libcbus1_shared = shared_library(
        'cbus1',
        objects: libcbus1_private.extract_all_objects(),
        dependencies: libcbus1_deps,
        install: not meson.is_subproject(),
        soversion: 0,
        link_depends: libcbus1_symfile,
        link_args: [
                '-Wl,--no-undefined',
                '-Wl,--version-script=@0@'.format(libcbus1_symfile),
        ],
)

libcbus1_dep = declare_dependency(
        include_directories: include_directories('.'),
        link_with: libcbus1_private,
        dependencies: libcbus1_deps,
        version: meson.project_version(),
)

if not meson.is_subproject()
        install_headers('c-bus1.h')

        mod_pkgconfig.generate(
                libraries: libcbus1_shared,
                version: meson.project_version(),
                name: 'libcbus1',
                filebase: 'libcbus1',
                description: project_description,
        )
endif

#
# target: test-*
#

test_api = executable('test-api', ['test-api.c'], link_with: libcbus1_shared)
test('API Symbol Visibility', test_api)

test_basic = executable('test-basic', ['test-basic.c'], dependencies: libcbus1_dep)
test('Basic API Behavior', test_basic)
