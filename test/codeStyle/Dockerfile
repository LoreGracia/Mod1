FROM alpine:3.22

RUN apk add --no-cache clang20-extra-tools bash git

RUN ln -sf /usr/lib/llvm20/bin/clang-format /usr/local/bin/clang-format && \
    ln -sf /usr/lib/llvm20/bin/clang-tidy   /usr/local/bin/clang-tidy

COPY scripts/ /opt/lint/

RUN chmod +x /opt/lint/run-lint.sh

WORKDIR /src