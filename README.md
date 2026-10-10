# simple-http-proxy-cpp

Proxy HTTP em C++17 com sockets crus, sem framework. Lê o header do
cliente, conecta no alvo, remove `Proxy-Connection` e `Connection`, e
repassa corpo e resposta em loop com `select`. Timeout por conexão e teto
de conexões simultâneas.

## Por que eu fiz isso

Já tinha cliente HTTP e servidor; faltava o meio. Proxy é onde aparece
conexão, header e streaming ao mesmo tempo — a peça que fecha rede.

## Como funciona

1. Aceita conexão em thread destacada, teto de 128.
2. Lê header até `\r\n\r\n`, limite 64 KB. Sem `Host` válido devolve 400.
3. Conecta no `host:port` do `Host`. Falha devolve 502.
4. Remove `Proxy-Connection`, `Connection` e `Keep-Alive`, repassa o resto.
5. `select` repassa nos dois sentidos com limite de 1 MB por conexão.

## Segurança

Sem TLS, então é proxy HTTP puro — não usar como proxy confiável de
tráfego sensível. Só repassa, não interpreta corpo. Host precisa casar
IPv4 válido via `inet_pton`. Teto de conexão e de tamanho evitam exaustão.

## Estrutura

    src/
      main.cpp       sockets, threads e relay
      proxy.cpp      parse de host e rebuild de header
    include/
      proxy.hpp      tipos e limites
    CMakeLists.txt
    README.md

## Como compilar

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    ./build/proxy 8893 127.0.0.1:8877

## Como rodar

    proxy <porta_de_escuta> <host:porta_alvo>

Cliente aponta o proxy e a requisição segue para o alvo.

## Testes validados

    target local com resposta fixa retorna o corpo através do proxy — OK
    Host ausente devolve 400 — OK
    alvo fechado devolve 502 — OK
    header grande acima de 64 KB é cortado — OK

## Limitações

Sem TLS, sem cache, sem compressão, sem HTTP/2, sem websocket. Relay
simples sem pool de conexões.

## Autor

Rafael Nunes — [@Rafaelnunes-alt2](https://github.com/Rafaelnunes-alt2)