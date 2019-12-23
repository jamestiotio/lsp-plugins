# Setup preferred tools
TOOL_LD                 = ld
TOOL_CC                 = gcc
TOOL_CXX                = g++
TOOL_PHP                = php

# Setup preferred flags
FLAG_RELRO              = -Wl,-z,relro,-z,now

# Patch flags and tools
ifeq ($(BUILD_PLATFORM),Solaris)
  FLAG_RELRO              =
  TOOL_LD                 = gld
endif

# Setup system variables
CC              		 ?= $(TOOL_CC)
CXX                      ?= $(TOOL_CXX)
PHP                      ?= $(TOOL_PHP)
LD                       ?= $(TOOL_LD)

MAKE_OPTS                 = -s
CFLAGS                   += $(CC_ARCH) -std=c++98 -fdata-sections -pthread -ffunction-sections -fno-exceptions -fno-asynchronous-unwind-tables -Wall -pipe -fno-rtti $(CC_FLAGS) -DLSP_MAIN_VERSION=\"$(VERSION)\" -DLSP_INSTALL_PREFIX=\"$(PREFIX)\"
CXXFLAGS                 += $(CC_ARCH) -std=c++98 -fdata-sections -pthread -ffunction-sections -fno-exceptions -fno-asynchronous-unwind-tables -Wall -pipe -fno-rtti $(CC_FLAGS) -DLSP_MAIN_VERSION=\"$(VERSION)\" -DLSP_INSTALL_PREFIX=\"$(PREFIX)\"
SO_FLAGS                  = $(CC_ARCH) $(FLAG_RELRO) -Wl,--gc-sections -shared -Llibrary -lc -fPIC
MERGE_FLAGS               = $(LD_ARCH) -r
EXE_TEST_FLAGS            = $(LDFLAGS) $(CC_ARCH)
EXE_FLAGS                 = $(LDFLAGS) $(CC_ARCH) $(FLAG_RELRO) -Wl,--gc-sections

ifneq ($(LD_PATH),)
  SO_FLAGS                 += -Wl,-rpath,$(LD_PATH)
  EXE_TEST_FLAGS           += -Wl,-rpath,$(LD_PATH)
  EXE_FLAGS                += -Wl,-rpath,$(LD_PATH)
endif

export CC
export CXX
export PHP
export LD

export MAKE_OPTS
export CFLAGS
export CXXFLAGS
export SO_FLAGS
export MERGE_FLAGS
export EXE_TEST_FLAGS
export EXE_FLAGS