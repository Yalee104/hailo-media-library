################################################
# APP WRAPPER SOURCE
################################################
sources = [
  'src/encoder/encoder.cpp',
  'src/frontend/frontend.cpp'
  ]

json_dep = meson.get_compiler('c').find_library('libnlohmann_json_schema_validator', required: true, dirs: '/usr/lib/')

encoder_lib = shared_library('hailo_medialibrary_api',
  sources,
  include_directories: include_directories('./include/'),
  dependencies : [gstreamer_deps, json_dep, gstmedialibrary_utils_dep, spdlog_dep, media_library_common_dep],
  version: meson.project_version(),
  gnu_symbol_visibility : 'default',
  install: true,
  install_dir: get_option('libdir'),
)

encoder_dep = declare_dependency(
  include_directories: [include_directories('./include/')],
  dependencies : [media_library_common_dep],
  link_with : encoder_lib)


gpiod_dep = dependency('libgpiod')


# STREAMING LIBRARY
gst_req = '>= 1.0.0'
gst_rtsp_server_dep = dependency('gstreamer-rtsp-server-1.0', version : gst_req)
gst_rtsp_dep = dependency('gstreamer-rtsp-1.0', version : gst_req)

sources_streaming = [
  'src/streaming/H15Streaming.cpp',
  ]

streaming_lib = static_library('hailo_streaming_api',
  sources_streaming,
  include_directories: include_directories('./include/streaming'),
  dependencies : [gstreamer_deps, json_dep, gst_rtsp_server_dep, gst_rtsp_dep],
  gnu_symbol_visibility : 'default',
  install: true,
  install_dir: get_option('libdir'),
)

streaming_dep = declare_dependency(
  include_directories: [include_directories('./include/')],
  link_with : streaming_lib)


# RESIZE LIBRARY
source_resize_crop = [
  'src/resize_crop/H15DspResizeCrop.cpp',
]

extern_resizecrop_lib = static_library('hailo_extern_resizecrop_api',
  source_resize_crop,
  include_directories: include_directories('./include/resize_crop'),
  dependencies : [gstmedialibrary_utils_dep, dsp_dep],
  gnu_symbol_visibility : 'default',
  install: true,
  install_dir: get_option('libdir'),
)

extern_resize_crop_dep = declare_dependency(
  include_directories: [include_directories('./include/')],
  link_with : extern_resizecrop_lib)


################################################
# Native Custom Example
################################################
native_incdir = include_directories(['examples/native_custom/', 'examples/native_custom/infra/', 'examples/native_custom/Others/' ])

native_custom_example_src = ['examples/native_custom/main.cpp']

executable('custom_full_inference_example',
  native_custom_example_src,
  dependencies : gstreamer_deps + [ media_library_frontend_dep, gstmedialibrary_utils_dep, encoder_dep, streaming_dep, 
                                    extern_resize_crop_dep, hailort_dep, opencv_dep],
  include_directories : native_incdir,
  gnu_symbol_visibility : 'default',
  install: true,
  install_dir: get_option('bindir'),
)


################################################
# Frontend Example
################################################
frontend_example_src = ['examples/frontend_example.cpp']

executable('frontend_example',
  frontend_example_src,
  dependencies : gstreamer_deps + [media_library_frontend_dep, gstmedialibrary_utils_dep, encoder_dep],
  gnu_symbol_visibility : 'default',
  install: true,
  install_dir: get_option('bindir'),
)

executable('jpeg_frontend_example',
  frontend_example_src,
  cpp_args: ['-DUSE_JPEG_JSONS'],
  dependencies : gstreamer_deps + [media_library_frontend_dep, gstmedialibrary_utils_dep, encoder_dep],
  gnu_symbol_visibility : 'default',
  install: true,
  install_dir: get_option('bindir'),
)


################################################
# Frontend with HRT Example
################################################

hailort_dep = [
    meson.get_compiler('cpp').find_library('hailort', required: true),
]
hailort_inc_dirs = include_directories(
    sysroot + '/usr/include/hailo',
    is_system: true
)


native_frontend_example_src = ['examples/native/main.cpp']

executable('native_frontend_example',
  native_frontend_example_src,
  include_directories: hailort_inc_dirs,
  dependencies : gstreamer_deps + [media_library_frontend_dep, gstmedialibrary_utils_dep, encoder_dep, hailort_dep, opencv_dep],
  gnu_symbol_visibility : 'default',
  install: true,
  install_dir: get_option('bindir'),
)


polygon_example_src = ['examples/polygon_example.cpp']

executable('polygon_example',
  polygon_example_src,
  dependencies : gstreamer_deps + [media_library_frontend_dep, gstmedialibrary_utils_dep, encoder_dep, opencv_dep],
  gnu_symbol_visibility : 'default',
  install: true,
  install_dir: get_option('bindir'),
)

calculate_size_osd_src = ['examples/calculate_text_size_osd.cpp']

executable('calc_osd_text_size',
  calculate_size_osd_src,
  dependencies : [opencv_dep, encoder_dep, gstmedialibrary_utils_dep],
  gnu_symbol_visibility : 'default',
  install: true,
  install_dir: get_option('bindir'),
)

install_subdir('include/media_library', install_dir: get_option('includedir') + '/hailo')
