# MiniTorrent

Torrent 파일 다운로드 시스템 구현

## 프로젝트 구조

```
.
├── src/              # 모든 소스 코드
│   ├── chunk_tool.c        # 파일 청킹 도구
│   ├── common.c/h          # 공통 헬퍼 함수
│   ├── downloader.c        # 다운로더
│   ├── peer_server.c       # Peer 서버
│   ├── register_peer.c     # Peer 등록
│   ├── tracker.c           # Tracker 서버
│   ├── sha256.c/h          # SHA256 해시 구현
│   └── verify_file.c       # 파일 무결성 검증
├── Makefile          # 빌드 설정
└── README.md         # 이 파일
```

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
