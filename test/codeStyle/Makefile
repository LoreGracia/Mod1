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

COMPOSE   := docker compose
LINT_DIRS := src inc Wrapper

.PHONY: lint lint-fix lint-summary lint-build lint-shell lint-clean

lint-build:
	@$(COMPOSE) build lint

lint: lint-build
	@$(COMPOSE) run --rm lint

lint-fix: lint-build
	@$(COMPOSE) run --rm lint-fix

lint-summary: lint-build
	@$(COMPOSE) run --rm -e MODE=summary lint

lint-shell: lint-build
	@$(COMPOSE) run --rm lint bash

lint-clean:
	@$(COMPOSE) down --rmi local
