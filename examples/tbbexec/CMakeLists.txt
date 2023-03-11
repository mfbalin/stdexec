#=============================================================================
# Copyright 2023 NVIDIA Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#=============================================================================

function(split pair name_out path_out)
    string(STRIP "${pair}" pair)
    string(REPLACE ":" ";" pair "${pair}")
    list(POP_FRONT pair _name)
    list(POP_FRONT pair _path)
    string(STRIP "${_name}" _name)
    string(STRIP "${_path}" _path)
    set(${name_out} "${_name}" PARENT_SCOPE)
    set(${path_out} "${_path}" PARENT_SCOPE)
endfunction()

function(def_example example)
    split(${example} target source)
    add_executable(${target} ${source})
    target_link_libraries(${target}
        PRIVATE STDEXEC::stdexec
                tbbexec
                stdexec_executable_flags)
endfunction()

set(tbbexec_examples
                       "example.scan  : scan.cpp"
)

foreach(example ${tbbexec_examples})
    def_example(${example})
endforeach()
