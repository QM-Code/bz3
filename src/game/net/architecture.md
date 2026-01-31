# src/game/net/architecture.md

Network flow:
1) Protobuf schema defines message wire format.
2) `proto_codec` converts between protobuf and internal structs.
3) Client/server sessions handle messages and update world state.
4) Transport backends (ENet) send/receive byte payloads.
