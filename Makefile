################################################################################
#                               COLOR DEFINITIONS                              #
################################################################################
END = \033[m
RED = \033[31m
GREEN = \033[32m
YELLOW = \033[33m
BLUE = \033[34m
LIGTH = \033[1m
DARK = \033[2m
ITALIC = \033[3m

SUCCESS = $(LIGTH)$(GREEN)[SUCCESS]$(END)
WARNING = $(LIGTH)$(YELLOW)[WARNING]$(END)
INFO = $(LIGTH)$(BLUE)[INFO]$(END)
ERROR = $(LIGTH)$(RED)[ERROR]$(END)

################################################################################
#                               SYSTEM                                         #
################################################################################

ifeq ($(OS),Windows_NT)
    UNAME_S := $(shell uname -s 2>NUL || echo Windows)

    ifneq (,$(findstring MINGW,$(UNAME_S)))
        PLATFORM := WINDOWS_MSYS
    else ifneq (,$(findstring MSYS,$(UNAME_S)))
        PLATFORM := WINDOWS_MSYS
    else ifneq (,$(findstring CYGWIN,$(UNAME_S)))
        PLATFORM := WINDOWS_CYGWIN
    else
        PLATFORM := WINDOWS_NATIVE
    endif
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        PLATFORM := LINUX
    else
        PLATFORM := UNIX_UNKNOWN
    endif
endif

ifeq ($(PLATFORM),LINUX)
    CXX      := g++
    LDLIBS   := -lGL -lX11 -lpthread -ldl
    CXXFLAGS += -DPLATFORM_LINUX -Wall -Werror -Wextra -g -pedantic -fsanitize=address
else ifeq ($(PLATFORM),WINDOWS_MSYS)
    CXX      := g++
    LDLIBS   := -lopengl32 -lgdi32
    CXXFLAGS += -DPLATFORM_WINDOWS
else
    $(error Unsupported platform: $(PLATFORM))
endif

DEBUG ?= 0
ifeq ($(DEBUG),1)
    CXXFLAGS += -DDEBUG=1
endif

################################################################################
#                          REQUIREMENTS (per platform)                         #
################################################################################

# ───── Common Toolchain ─────
ifeq ($(PLATFORM),LINUX)
    REQUIRED_TOOLS   := g++ make ld pkg-config
    REQUIRED_PKGS    := gl x11
    REQUIRED_HEADERS := /usr/include/X11/Xlib.h /usr/include/GL/glx.h /usr/include/GL/gl.h
    CHECK_DISPLAY    := yes
else ifeq ($(PLATFORM),WINDOWS_MSYS)
    REQUIRED_TOOLS   := g++ make ld
    REQUIRED_PKGS    :=
    REQUIRED_HEADERS :=
    CHECK_DISPLAY    := no
else ifeq ($(PLATFORM),WINDOWS_CYGWIN)
    REQUIRED_TOOLS   := g++ make ld
    REQUIRED_PKGS    :=
    REQUIRED_HEADERS :=
    CHECK_DISPLAY    := no
else
    REQUIRED_TOOLS   :=
    REQUIRED_PKGS    :=
    REQUIRED_HEADERS :=
    CHECK_DISPLAY    := no
endif

# ───── Actual project dependencies (what main.cpp uses) ─────
# main.cpp includes <X11/Xlib.h> and <GL/glx.h>: it only compiles on Linux.
PROJECT_PROBE_SRC  := \#include <X11/Xlib.h>\n\#include <GL/glx.h>\nint main(void){return 0;}
PROJECT_PROBE_LIBS := -lGL -lX11 -lpthread -ldl

# ───── Probe de toolchain (portable) ─────
TOOLCHAIN_PROBE_SRC := int main(void){return 0;}

################################################################################
#                               BUILD VARIABLES                                #
################################################################################

NAME = mod1
RMV = rm -rf
CLEAR = $(OBJ_DIR) $(DEP_DIR)

# Directories
SRC_DIR = src
TEST_DIR = tests
OBJ_DIR = .obj
DEP_DIR = .dep

SUB_DIR_OBJ =	$(OBJ_DIR)/$(EXTER_DIR) $(OBJ_DIR)/$(GLDIR) \
                $(OBJ_DIR)/$(WRAPPER_DIR) $(OBJ_DIR)/$(CORE_DIR) \
                $(OBJ_DIR)/$(MATH_DIR) $(OBJ_DIR)/$(RENDER_DIR) \
                $(OBJ_DIR)/$(SCENE_DIR)

SUB_DIR_DEP =	$(DEP_DIR)/$(EXTER_DIR) $(DEP_DIR)/$(GLDIR) \
                $(DEP_DIR)/$(WRAPPER_DIR) $(DEP_DIR)/$(CORE_DIR) \
                $(DEP_DIR)/$(MATH_DIR) $(DEP_DIR)/$(RENDER_DIR) \
                $(DEP_DIR)/$(SCENE_DIR)

EXTER_DIR = external
GLDIR = $(EXTER_DIR)/glad/src

WRAPPER_DIR = Wrapper
CORE_DIR = $(WRAPPER_DIR)/core
MATH_DIR = $(WRAPPER_DIR)/math
RENDER_DIR = $(WRAPPER_DIR)/render
SCENE_DIR = $(WRAPPER_DIR)/scene

SOURCES := main.cpp
GLSOURCES := $(GLDIR)/gl.c $(GLDIR)/glx.c
WRAPSOURCES :=	$(CORE_DIR)/WWindow.cpp $(CORE_DIR)/Environment.cpp $(CORE_DIR)/Input.cpp \
                $(RENDER_DIR)/Shader.cpp $(RENDER_DIR)/Mesh.cpp $(RENDER_DIR)/Renderer.cpp \
                $(SCENE_DIR)/Camera.cpp $(SCENE_DIR)/Transform.cpp $(SCENE_DIR)/Entity.cpp $(SCENE_DIR)/Scene.cpp

INCLUDES := $(addprefix -I, ./inc)
INCLUDES += $(addprefix -I, ./external/glm)
INCLUDES += $(addprefix -I, ./external/glm/detail)
INCLUDES += $(addprefix -I, ./external/glm/ext)
INCLUDES += $(addprefix -I, ./external/glm/gtc)
INCLUDES += $(addprefix -I, ./external/glm/gtx)
INCLUDES += $(addprefix -I, ./external/glm/simd)
INCLUDES += $(addprefix -I, ./external/glad/include/)
INCLUDES += $(addprefix -I, ./Wrapper)
INCLUDES += $(addprefix -I, ./Wrapper/core)
INCLUDES += $(addprefix -I, ./Wrapper/math)
INCLUDES += $(addprefix -I, ./Wrapper/render)
INCLUDES += $(addprefix -I, ./Wrapper/scene)

OBJS =  $(addprefix $(OBJ_DIR)/, $(SOURCES:.cpp=.o))
DEPS = $(addprefix $(DEP_DIR)/, $(SOURCES:.cpp=.d))

GLOBJS = $(addprefix $(OBJ_DIR)/, $(GLSOURCES:.c=.o))
GLDEP = $(addprefix $(OBJ_DIR)/, $(GLSOURCES:.c=.d))

WRAPOBJS = $(addprefix $(OBJ_DIR)/, $(WRAPSOURCES:.cpp=.o))
WRAPDEP = $(addprefix $(DEP_DIR)/, $(WRAPSOURCES:.cpp=.d))

################################################################################
#                               CODE STYLE (DOCKER)                            #
################################################################################

COMPOSE_FILE  := test/codeStyle/docker-compose.yml
COMPOSE_CMD   := docker compose -f $(COMPOSE_FILE)
LINT_DIRS     ?= src inc Wrapper
LINT_SERVICE  := lint
LINT_FIX_SVC  := lint-fix

################################################################################
#                               PHONY TARGETS                                  #
################################################################################
.PHONY: help clean fclean re \
        lint lint-build lint-fix lint-summary lint-shell lint-clean

################################################################################
#                               DEFAULT TARGET                                 #
################################################################################
all: check_requirements $(NAME) ## Deafault rule, compiles all. Called with make, also.

################################################################################
#                               HELP TARGET                                    #
################################################################################
help: ## Display this help message with available targets.
    @echo "$(INFO) Available targets:$(END)"
    @awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z_-]+:.*?## / \
    {printf "  $(LIGTH)%-20s$(END) %s\n", $$1, $$2}' $(MAKEFILE_LIST)

################################################################################
#                          REQUIREMENTS CHECK                                  #
################################################################################

CHECK ?= 0

ifeq ($(CHECK),1)
    $(NAME): check_requirements
endif

check_requirements: ## Verify that the system has everything needed to build
    @printf "$(INFO) Checking requirements on $(PLATFORM)...\n"
    @failed=0; \
    for tool in $(REQUIRED_TOOLS); do \
        if command -v $$tool >/dev/null 2>&1; then \
            printf "$(SUCCESS) tool '$$tool' found.\n"; \
        else \
            printf "$(ERROR) tool '$$tool' MISSING.\n"; failed=1; \
        fi; \
    done; \
    if [ -n "$(REQUIRED_PKGS)" ]; then \
        for pkg in $(REQUIRED_PKGS); do \
            if pkg-config --exists $$pkg 2>/dev/null; then \
                printf "$(SUCCESS) pkg '$$pkg' found.\n"; \
            else \
                printf "$(ERROR) pkg '$$pkg' MISSING (install the -dev package).\n"; failed=1; \
            fi; \
        done; \
    fi; \
    for hdr in $(REQUIRED_HEADERS); do \
        if [ -f "$$hdr" ]; then \
            printf "$(SUCCESS) header '$$hdr' found.\n"; \
        else \
            printf "$(ERROR) header '$$hdr' MISSING.\n"; failed=1; \
        fi; \
    done; \
    if [ "$(CHECK_DISPLAY)" = "yes" ]; then \
        if [ -n "$$DISPLAY" ]; then \
            printf "$(SUCCESS) DISPLAY=$$DISPLAY available.\n"; \
        else \
            printf "$(ERROR) DISPLAY is empty (no X server).\n"; failed=1; \
        fi; \
    fi; \
    if [ $$failed -ne 0 ]; then \
        printf "$(ERROR) Missing toolchain requirements. Aborting.\n"; exit 1; \
    fi; \
    printf "$(SUCCESS) Toolchain requirements OK.\n"
    @printf "$(INFO) Probing toolchain (compile+link)...\n"
    @tmp=$$(mktemp -d); \
    printf '$(TOOLCHAIN_PROBE_SRC)\n' > $$tmp/probe.cpp; \
    if $(CXX) $$tmp/probe.cpp -o $$tmp/probe >/dev/null 2>&1; then \
        printf "$(SUCCESS) Toolchain compile+link OK.\n"; \
    else \
        printf "$(ERROR) Toolchain compile+link FAILED.\n"; \
        $(CXX) $$tmp/probe.cpp -o $$tmp/probe; \
        rm -rf $$tmp; exit 1; \
    fi; \
    rm -rf $$tmp
    @printf "$(INFO) Probing project dependencies (X11 + GLX)...\n"
    @tmp=$$(mktemp -d); \
    printf '$(PROJECT_PROBE_SRC)\n' > $$tmp/probe.cpp; \
    if $(CXX) $$tmp/probe.cpp -o $$tmp/probe $(PROJECT_PROBE_LIBS) >/dev/null 2>&1; then \
        printf "$(SUCCESS) Project dependencies (X11 + GLX) OK.\n"; \
    else \
        printf "$(ERROR) Project dependencies (X11 + GLX) FAILED.\n"; \
        if [ "$(PLATFORM)" != "LINUX" ]; then \
            printf "$(ERROR) This project uses X11/GLX and only builds on Linux.\n"; \
            printf "$(WARNING) On Windows, port to WGL or use GLFW/SDL.\n"; \
        fi; \
        $(CXX) $$tmp/probe.cpp -o $$tmp/probe $(PROJECT_PROBE_LIBS); \
        rm -rf $$tmp; exit 1; \
    fi; \
    rm -rf $$tmp

################################################################################
#                               DIRECTORY CREATION                             #
################################################################################

define create_dirs
    @for dir in $(1); do \
        if [ -d "$$dir" ]; then \
            printf " -- The directory already exists: %s\n" "$$dir"; \
        else \
            mkdir -p "$$dir" && printf " -- Created directory: %s\n" "$$dir"; \
        fi; \
    done
endef

$(OBJ_DIR):
    $(call create_dirs, $(OBJ_DIR) $(SUB_DIR_OBJ))
    @printf "$(INFO) Created object directory structure\n"

$(DEP_DIR):
    $(call create_dirs, $(DEP_DIR) $(SUB_DIR_DEP))
    @printf "$(INFO) Created dependencies directory structure\n"

################################################################################
#                               MOD BUILD RULES                                #
################################################################################

define compile_cpp
    @printf "$(INFO) files:  $< ...\n"
    @if $(CXX) $(CXXFLAGS) $(INCLUDES) \
        -MMD -MF $(DEP_DIR)/$(1)$*.d -MT $@ \
        -c $< -o $@; then \
        printf "$(SUCCESS) Created: $@\n"; \
    else \
        printf "$(ERROR) Failed to compile: $<\n"; \
        exit 1; \
    fi
endef

define compile_c
    @printf "$(INFO) files:  $< ...\n"
    @if gcc $(INCLUDES) -MMD -MF $(DEP_DIR)/$(1)$*.d -MT $@ \
        -c $< -o $@; then \
        printf "$(SUCCESS) Created: $@\n"; \
    else \
        printf "$(ERROR) Failed to compile: $<\n"; \
        exit 1; \
    fi
endef

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(OBJ_DIR) | $(DEP_DIR)
    $(call compile_cpp,)

$(OBJ_DIR)/$(WRAPPER_DIR)/%.o: $(WRAPPER_DIR)/%.cpp $(OBJ_DIR) | $(DEP_DIR)
    $(call compile_cpp,$(WRAPPER_DIR)/)

$(OBJ_DIR)/external/glad/src/%.o: $(GLDIR)/%.c $(OBJ_DIR) | $(DEP_DIR)
    $(call compile_c,$(GLDIR)/)

$(NAME): $(OBJS) $(GLOBJS) $(WRAPOBJS)
    @ if $(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJS) $(GLOBJS) $(WRAPOBJS) $(LDLIBS) -o $(NAME); then \
        printf "$(SUCCESS) Created program: $(NAME)\n"; \
    else \
        printf "$(ERROR) when generated $(NAME)"; \
        exit 1; \
    fi

################################################################################
#                               CODE STYLE TARGETS                             #
################################################################################

lint-build: ## Build the clang-format / clang-tidy Docker image
    @$(COMPOSE_CMD) build $(LINT_SERVICE)

lint: lint-build ## Run clang-format and clang-tidy (read-only, detailed diff)
    @$(COMPOSE_CMD) run --rm -e LINT_DIRS="$(LINT_DIRS)" $(LINT_SERVICE)

lint-summary: lint-build ## Run linter in summary mode (counters only)
    @$(COMPOSE_CMD) run --rm -e LINT_DIRS="$(LINT_DIRS)" -e MODE=summary $(LINT_SERVICE)

lint-fix: lint-build ## Apply clang-format -i to all files (modifies sources)
    @$(COMPOSE_CMD) run --rm -e LINT_DIRS="$(LINT_DIRS)" -e MODE=fix $(LINT_FIX_SVC)

lint-shell: lint-build ## Open an interactive shell inside the linter container
    @$(COMPOSE_CMD) run --rm $(LINT_SERVICE) bash

lint-clean: ## Remove the linter Docker image and containers
    @$(COMPOSE_CMD) down --rmi local

################################################################################
#                               CLEANUP TARGETS                                #
################################################################################
clean: ## Remove all build objects, test runner, and kernel binary.
    @printf "$(INFO) Cleaning build artifacts...\n"
    @$(foreach dir,$(CLEAR),\
        if [ -d "$(dir)" ]; then printf "  Remove  $(dir)\n"; rm -rf "$(dir)"; \
        else printf "$(WARNING) Object directory: $(dir) not found\n";\
        fi;)
    @printf "$(SUCCESS) Clean complete\n"

fclean: clean ## Perform a full cleanup
    @printf "$(INFO) Cleaning all generated files...\n"
    @if [ -f $(NAME) ]; then \
        printf " Remove program: $(NAME)\n"; \
        $(RMV) $(NAME); \
    else \
        printf "$(WARNING) Program: $(NAME) not found\n"; \
    fi;

################################################################################
#                               REBUILD TARGET                                 #
################################################################################
re: fclean all ## Rebuild everything from mod1 (equivalent to fclean followed by all).

-include $(DEPS)
-include $(GLDEP)
-include $(WRAPDEP)
.SILENT: