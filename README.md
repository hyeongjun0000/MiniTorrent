# 🚀 MiniTorrent: TCP 기반 분산 파일 공유 시스템

> **운영체제 및 네트워크 성능 분석 시뮬레이터 프로젝트** > 단일 서버 병목 현상을 해결하기 위해 고안된 P2P(Peer-to-Peer) 기반의 멀티스레드 파일 병렬 다운로드 시스템입니다.

##  프로젝트 개요
기존의 Client-Server 모델은 대용량 파일 전송 시 서버의 대역폭 한계로 인해 치명적인 병목 현상이 발생합니다. 본 프로젝트는 이 문제를 해결하기 위해 파일 조각(Chunk) 단위 분할, Tracker 기반의 피어 탐색, 그리고 다중 소켓 멀티스레딩을 활용한 **응용 계층 P2P 프로토콜**을 C언어로 밑바닥부터 직접 설계하고 구현한 시뮬레이터입니다.

## 시스템 아키텍처
본 시스템은 6개의 독립적인 컴포넌트(실행 파일)로 모듈화되어 있으며, TCP 소켓을 통해 유기적으로 상호작용합니다.

1. **`tracker`**: 전체 네트워크의 연락망 역할을 하는 중앙 제어 서버 (Peer Node 매핑)
2. **`peer_server`**: 자신이 보유한 파일 조각을 다른 클라이언트에게 업로드(Seed)하는 데몬
3. **`downloader`**: Tracker로부터 목록을 받아 다중 Peer와 동시 다발적으로 연결하여 병렬 다운로드를 수행하는 클라이언트
4. **`register_peer`**: Peer의 보유 Chunk 정보를 Tracker에 등록하는 CLI 유틸리티
5. **`chunk_tool`**: 대용량 파일을 지정된 크기로 분할(Split)하거나, 수신된 조각들을 병합(Merge)하는 도구
6. **`verify_file`**: 분할 전 원본 파일과 다운로드/병합 완료된 파일의 SHA-256 해시를 대조하여 무결성을 검증하는 도구
## 빌드

```bash
make
```

생성되는 실행 파일:
- `tracker` - Tracker 서버
- `peer_server` - Peer 서버
- `downloader` - 파일 다운로더
- `chunk_tool` - 파일 청킹 도구
- `register_peer` - Peer 등록 도구
- `verify_file` - 파일 무결성 검증 도구

## 정리

```bash
make clean
```

## 기본 사용법

### 1. 파일 청킹
```bash
./chunk_tool <source_file> <chunk_size>
```

### 2. Tracker 실행
```bash
./tracker
```

### 3. Peer 등록 및 실행
```bash
./register_peer <tracker_host> <peer_host> <peer_port>
./peer_server <tracker_host> <peer_port>
```

### 4. 파일 다운로드
```bash
./downloader <tracker_host> <file_name> <output_dir>
```

### 5. 무결성 검증
```bash
./verify_file <original_file> <downloaded_file>
```
