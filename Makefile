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

ifneq (,$(filter Windows_NT,$(OS)))
    PLATFORM := WINDOWS
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        PLATFORM := LINUX
    else
        PLATFORM := UNIX_UNKNOWN
    endif
endif

ifeq ($(PLATFORM),UNIX_UNKNOWN)
    $(error Unsupported platform: $(UNAME_S))
endif

ifeq ($(PLATFORM),WINDOWS)
    SHELL := sh
    .SHELLFLAGS := -c
endif

################################################################################
#                               BUILD VARIABLES                                #
################################################################################

NAME = mod1
RMV = rm -rf
CLEAR = $(OBJ_DIR) $(DEP_DIR)

SRC_DIR = src
OBJ_DIR = .obj
DEP_DIR = .dep

EXTER_DIR = external
GLDIR = $(EXTER_DIR)/glad/src
GLFW_VENDOR_DIR = $(EXTER_DIR)/glfw

WRAPPER_DIR = Wrapper
CORE_DIR = $(WRAPPER_DIR)/core
MATH_DIR = $(WRAPPER_DIR)/math
RENDER_DIR = $(WRAPPER_DIR)/render
SCENE_DIR = $(WRAPPER_DIR)/scene

SUB_DIR_OBJ =	$(OBJ_DIR)/$(EXTER_DIR) $(OBJ_DIR)/$(GLDIR) \
                $(OBJ_DIR)/$(WRAPPER_DIR) $(OBJ_DIR)/$(CORE_DIR) \
                $(OBJ_DIR)/$(MATH_DIR) $(OBJ_DIR)/$(RENDER_DIR) \
                $(OBJ_DIR)/$(SCENE_DIR)

SUB_DIR_DEP =	$(DEP_DIR)/$(EXTER_DIR) $(DEP_DIR)/$(GLDIR) \
                $(DEP_DIR)/$(WRAPPER_DIR) $(DEP_DIR)/$(CORE_DIR) \
                $(DEP_DIR)/$(MATH_DIR) $(DEP_DIR)/$(RENDER_DIR) \
                $(DEP_DIR)/$(SCENE_DIR)

SOURCES := main.cpp
GLSOURCES := $(GLDIR)/gl.c
WRAPSOURCES :=	$(CORE_DIR)/WWindow.cpp $(CORE_DIR)/Environment.cpp $(CORE_DIR)/Input.cpp \
                $(RENDER_DIR)/Shader.cpp $(RENDER_DIR)/Mesh.cpp $(RENDER_DIR)/Renderer.cpp \
                $(SCENE_DIR)/Camera.cpp $(SCENE_DIR)/Transform.cpp $(SCENE_DIR)/Entity.cpp $(SCENE_DIR)/Scene.cpp

INCLUDES := $(addprefix -I, ./external/glm)
INCLUDES += $(addprefix -I, ./external/glm/detail)
INCLUDES += $(addprefix -I, ./external/glm/ext)
INCLUDES += $(addprefix -I, ./external/glm/gtc)
INCLUDES += $(addprefix -I, ./external/glm/simd)
INCLUDES += $(addprefix -I, ./external/glad/include/)
INCLUDES += $(addprefix -I, ./$(GLFW_VENDOR_DIR)/include)
INCLUDES += $(addprefix -I, ./Wrapper)
INCLUDES += $(addprefix -I, ./Wrapper/core)
INCLUDES += $(addprefix -I, ./Wrapper/math)
INCLUDES += $(addprefix -I, ./Wrapper/render)
INCLUDES += $(addprefix -I, ./Wrapper/scene)

OBJS =  $(addprefix $(OBJ_DIR)/, $(SOURCES:.cpp=.o))
DEPS = $(addprefix $(DEP_DIR)/, $(SOURCES:.cpp=.d))

GLOBJS = $(addprefix $(OBJ_DIR)/, $(GLSOURCES:.c=.o))
GLDEP = $(addprefix $(DEP_DIR)/, $(GLSOURCES:.c=.d))

WRAPOBJS = $(addprefix $(OBJ_DIR)/, $(WRAPSOURCES:.cpp=.o))
WRAPDEP = $(addprefix $(DEP_DIR)/, $(WRAPSOURCES:.cpp=.d))

################################################################################
#                               GLFW                                           #
################################################################################

ifeq ($(PLATFORM),LINUX)
    CXX      := g++
    CC       := gcc
    CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -g -pedantic
    CFLAGS   := -std=c99 -O2

    GLFW_PKG_CONFIG_PATH := $(GLFW_VENDOR_DIR)/lib/pkgconfig

    ifeq ($(strip $(GLFW_CFLAGS)),)
        GLFW_CFLAGS := $(shell PKG_CONFIG_PATH="$(GLFW_PKG_CONFIG_PATH):$${PKG_CONFIG_PATH:-}" pkg-config --cflags glfw3 2>/dev/null)
    endif

    ifeq ($(strip $(GLFW_LIBS)),)
        GLFW_LIBS := $(shell PKG_CONFIG_PATH="$(GLFW_PKG_CONFIG_PATH):$${PKG_CONFIG_PATH:-}" pkg-config --libs glfw3 2>/dev/null)
    endif

    LDLIBS := $(GLFW_LIBS)

    SANITIZE ?= 0
    ifeq ($(SANITIZE),1)
        CXXFLAGS += -fsanitize=address
    endif
else ifeq ($(PLATFORM),WINDOWS)
    CXX      := g++
    CC       := gcc
    CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -g -pedantic
    CFLAGS   := -std=c99 -O2

    GLFW_CFLAGS ?= -I$(GLFW_VENDOR_DIR)/include
    GLFW_LIBS   ?= -L$(GLFW_VENDOR_DIR)/lib-mingw-w64 -lglfw3 -lopengl32 -lgdi32
    LDLIBS      := $(GLFW_LIBS)
endif

CXXFLAGS += $(GLFW_CFLAGS)

DEBUG ?= 0
ifeq ($(DEBUG),1)
    CXXFLAGS += -DDEBUG=1
endif

################################################################################
#                          REQUIREMENTS (per platform)                         #
################################################################################

ifeq ($(PLATFORM),LINUX)
    REQUIRED_TOOLS   := g++ gcc make ld pkg-config
else
    REQUIRED_TOOLS   := g++ gcc ld
endif

################################################################################
#                               CODE STYLE (DOCKER)                            #
################################################################################

COMPOSE_FILE  := test/codeStyle/docker-compose.yml
COMPOSE_CMD   := docker compose -f $(COMPOSE_FILE)
LINT_DIRS     ?= src Wrapper
LINT_SERVICE  := lint
LINT_FIX_SVC  := lint-fix

################################################################################
#                               PHONY TARGETS                                  #
################################################################################
.PHONY: help clean fclean re check_requirements glfw-linux glfw-mingw \
        lint lint-build lint-fix lint-summary lint-shell lint-clean

################################################################################
#                               DEFAULT TARGET                                 #
################################################################################
all: check_requirements $(NAME) ## Default rule, compiles all. Called with make, also.

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
    if command -v pkg-config >/dev/null 2>&1 && PKG_CONFIG_PATH="$(GLFW_PKG_CONFIG_PATH):$${PKG_CONFIG_PATH:-}" pkg-config --exists glfw3 2>/dev/null; then \
        printf "$(SUCCESS) pkg 'glfw3' found ($$(PKG_CONFIG_PATH="$(GLFW_PKG_CONFIG_PATH):$${PKG_CONFIG_PATH:-}" pkg-config --modversion glfw3 2>/dev/null)).\n"; \
    elif [ -f "$(GLFW_VENDOR_DIR)/include/GLFW/glfw3.h" ]; then \
	    printf "$(SUCCESS) vendored GLFW headers found in $(GLFW_VENDOR_DIR)/include.\n"; \
	else \
	    printf "$(ERROR) GLFW not found. Install libglfw3-dev (Linux) or run scripts/setup_glfw_mingw.sh (Windows).\n"; failed=1; \
	fi; \
	if [ "$(PLATFORM)" = "WINDOWS" ]; then \
	    if [ ! -f "$(GLFW_VENDOR_DIR)/lib-mingw-w64/libglfw3.a" ] && [ ! -f "$(GLFW_VENDOR_DIR)/lib-mingw-w64/glfw3.dll.a" ]; then \
	        printf "$(ERROR) GLFW MinGW libraries MISSING in $(GLFW_VENDOR_DIR)/lib-mingw-w64.\n"; failed=1; \
	    fi; \
	fi; \
	if [ $$failed -ne 0 ]; then \
	    printf "$(ERROR) Missing toolchain requirements. Aborting.\n"; exit 1; \
	fi; \
	printf "$(SUCCESS) Toolchain requirements OK.\n"
	@printf "$(INFO) Probing GLFW + GLAD (compile+link)...\n"
	@tmp=$$(mktemp -d); \
	printf '#define GLFW_INCLUDE_NONE\n#include <GLFW/glfw3.h>\n#include <glad/gl.h>\nint main(void){return 0;}\n' > $$tmp/probe.cpp; \
	if $(CXX) $(CXXFLAGS) $(INCLUDES) $$tmp/probe.cpp -o $$tmp/probe $(LDLIBS) >/dev/null 2>&1; then \
	    printf "$(SUCCESS) GLFW + GLAD compile+link OK.\n"; \
	else \
	    printf "$(ERROR) GLFW + GLAD compile+link FAILED.\n"; \
	    $(CXX) $(CXXFLAGS) $(INCLUDES) $$tmp/probe.cpp -o $$tmp/probe $(LDLIBS); \
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
	@if $(CC) $(CFLAGS) $(INCLUDES) -MMD -MF $(DEP_DIR)/$(1)$*.d -MT $@ \
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
#                               GLFW HELPERS                                   #
################################################################################

glfw-linux: ## Install/build GLFW on Debian/Ubuntu (pkg-config or vendored build)
	@./scripts/setup_glfw_linux.sh

glfw-mingw: ## Download and vendor GLFW MinGW-w64 binaries in external/glfw
	@./scripts/setup_glfw_mingw.sh

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
