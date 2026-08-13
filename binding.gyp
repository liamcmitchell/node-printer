{
  "variables": {
    "module_name%": "node_printer",
    "module_path%": "lib",
    "openssl_fips": ""
  },
  'targets': [
    {
      "target_name": "action_after_build",
      "type": "none",
      "dependencies": [ "<(module_name)" ],
      "copies": [
        {
          "files": [ "<(PRODUCT_DIR)/<(module_name).node" ],
          "destination": "<(module_path)"
        }
      ]
    },
    {
      'target_name': 'node_printer',
      'sources': [
        'src/node_printer.cc',
        'src/printer_model.cc',
        'src/node_printer_posix.cc',
        'src/node_printer_win.cc'
      ],
      'include_dirs' : [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      'dependencies': [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      'defines': [
        'NODE_ADDON_API_DISABLE_CPP_EXCEPTIONS'
      ],
      'cflags_cc+': [
        "-Wno-deprecated-declarations"
      ],
      'conditions': [
        # common exclusions
        ['OS!="linux"', {'sources/': [['exclude', '_linux\\.cc$']]}],
        ['OS!="mac"', {'sources/': [['exclude', '_mac\\.cc|mm?$']]}],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']]}, {
          # else if OS==win, exclude also posix files
          'sources/': [['exclude', '_posix\\.cc$']]
        }],
        # specific settings
        ['OS!="win"', {
          'cflags':[
            '<!(cups-config --cflags)'
          ],
          'ldflags':[
            '<!(cups-config --libs)'
            #'-lcups -lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err -lz -lpthread -lm -lcrypt -lz'
          ],
          'libraries':[
            '<!(cups-config --libs)'
            #'-lcups -lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err -lz -lpthread -lm -lcrypt -lz'
          ],
          'link_settings': {
            'libraries': [
              '<!(cups-config --libs)'
            ]
          }
        }],
        ['OS=="win"', {
          "defines": [
            "NOMINMAX" # allow std::min/max to work
          ],
          "cflags": [
            "-O2"
          ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "AdditionalOptions": [ "-std:c++20", ],
            },
          },
        }],
        ['OS=="mac"', {
          'cflags':[
            "-stdlib=libc++"
          ],
          'xcode_settings': {
            "OTHER_CPLUSPLUSFLAGS":["-std=c++20", "-stdlib=libc++"],
            "OTHER_LDFLAGS": ["-stdlib=libc++"],
            "MACOSX_DEPLOYMENT_TARGET": "14.0",
          },
        }],
        ['OS=="linux"', {
          'cflags_cc':[
            "-std=c++20",
            # Electron 43 headers can trigger a GCC parse error on declarations
            # combining [[deprecated(...)]] and V8_EXPORT visibility attributes.
            # Undefining V8_DEPRECATION_WARNINGS disables those deprecated attrs.
            "-UV8_DEPRECATION_WARNINGS"
          ]
        }],
      ]
    }
  ]
}
