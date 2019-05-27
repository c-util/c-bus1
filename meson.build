project(
        'c-bus1',
        'c',
        version: '1',
        license: 'Apache',
        default_options: [
                'c_std=c11'
        ],
)
project_description = 'Bus1 Capability-based IPC Bindings for ISO-C11'

add_project_arguments('-D_GNU_SOURCE', language: 'c')
mod_pkgconfig = import('pkgconfig')

sub_clist = subproject('c-list')
sub_crbtree = subproject('c-rbtree')
sub_cstdaux = subproject('c-stdaux')

dep_clist = sub_clist.get_variable('libclist_dep')
dep_crbtree = sub_crbtree.get_variable('libcrbtree_dep')
dep_cstdaux = sub_cstdaux.get_variable('libcstdaux_dep')

subdir('src')
