# MiniTorrent: TCP 기반 분산 파일 공유 시스템

> **운영체제 및 네트워크 성능 분석 시뮬레이터 프로젝트**
> 단일 서버 병목 현상을 해결하기 위해 고안된 P2P 기반의 멀티스레드 파일 병렬 다운로드 시스템

##  프로젝트 개요
기존의 Client-Server 모델은 대용량 파일 전송 시 서버의 대역폭 한계로 인해 치명적인 병목 현상이 발생함. 본 프로젝트는 이 문제를 해결하기 위해 파일 조각(Chunk) 단위 분할, Tracker 기반의 피어 탐색, 그리고 다중 소켓 멀티스레딩을 활용한 **응용 계층 P2P 프로토콜**을 C언어로 밑바닥부터 직접 설계하고 구현한 시뮬레이터이다.

## 시스템 아키텍처
본 시스템은 6개의 독립적인 컴포넌트실행 파일로 모듈화되어 있으며, TCP 소켓을 통해 유기적으로 상호작용함.

1. **`tracker`**: 전체 네트워크의 연락망 역할을 하는 중앙 제어 서버 (Peer Node 매핑)
2. **`peer_server`**: 자신이 보유한 파일 조각을 다른 클라이언트에게 업로드하는 데몬
3. **`downloader`**: Tracker로부터 목록을 받아 다중 Peer와 동시 다발적으로 연결하여 병렬 다운로드를 수행하는 클라이언트
4. **`register_peer`**: Peer의 보유 Chunk 정보를 Tracker에 등록하는 CLI 유틸리티
5. **`chunk_tool`**: 대용량 파일을 지정된 크기로 분할하거나, 수신된 조각들을 병합Merge하는 도구
6. **`verify_file`**: 분할 전 원본 파일과 다운로드/병합 완료된 파일의 SHA-256 해시를 대조하여 무결성을 검증하는 도구

## 빌드

```bash
make
```

## 정리

```bash
make clean
```

## 테스트 시나리오

sequenceDiagram
    actor Client as Download Peer
    participant Tracker as Tracker (Port 7200)
    participant Seed1 as Seed 1 (Port 9201)
    participant Seed2 as Seed 2 (Port 9202)

    Note over Seed1, Seed2: Precondition: Chunks distributed
    
    %% Step 1: Register
    Seed1->>Tracker: REGISTER (source.bin, Chunks: 0,1,2)
    Tracker-->>Seed1: OK REGISTERED
    Seed2->>Tracker: REGISTER (source.bin, Chunks: 3,4,5)
    Tracker-->>Seed2: OK REGISTERED

    %% Step 2: Query
    Client->>Tracker: QUERY source.bin
    Tracker-->>Client: PEERS 2 (Seed1: 0~2, Seed2: 3~5)

    %% Step 3: Parallel Download (Multithreading)
    Note over Client, Seed2: Multi-threaded Parallel Download
    par Thread 1~3 (To Seed 1)
        Client->>Seed1: GET source.bin (Chunk 0,1,2)
        Seed1-->>Client: Send Chunk 0,1,2 Data
    and Thread 4~6 (To Seed 2)
        Client->>Seed2: GET source.bin (Chunk 3,4,5)
        Seed2-->>Client: Send Chunk 3,4,5 Data
    end

    %% Step 4: Merge & Verify
    Note over Client: All Chunks Received
    Client->>Client: Merge Chunks (rebuilt.bin)
    Client->>Client: Verify SHA-256 (Original vs Rebuilt)
    Note right of Client: SHA256 MATCH (Success)
    
**Step 1: 환경 초기화 및 더미 데이터 분할**
이전 테스트의 잔여물을 제거하고, 6MB 크기의 더미 파일을 생성하여 1MB 단위 조각(Chunk)으로 분할 및 분배
```bash
# 1. 빌드 및 기존 테스트 파일 초기화
make
rm -rf source_chunks seed1_chunks seed2_chunks downloaded_multi rebuilt.bin source.bin

# 2. 6MB 원본 파일 생성 및 1MB 단위 분할
dd if=/dev/urandom of=source.bin bs=1M count=6
./chunk_tool split source.bin 1048576 source_chunks

# 3. Seed 디렉토리 생성 및 조각 파일 명시적 분배
mkdir -p seed1_chunks seed2_chunks

# Seed 1: 0~2번 조각 할당
cp source_chunks/source.bin.part0000 seed1_chunks/
cp source_chunks/source.bin.part0001 seed1_chunks/
cp source_chunks/source.bin.part0002 seed1_chunks/

# Seed 2: 3~5번 조각 할당
cp source_chunks/source.bin.part0003 seed2_chunks/
cp source_chunks/source.bin.part0004 seed2_chunks/
cp source_chunks/source.bin.part0005 seed2_chunks/
```
**Step 2: P2P 네트워크 인프라 가동**
아래의 명령어들은 각각 새로운 터미널 창을 열어 프로젝트 폴더로 이동한 뒤 독립적으로 실행해야 함.

```Bash
# [창 1] 중앙 제어 Tracker 서버 가동 (Port: 7200)
./tracker 7200

# [창 2] Seed 1 서버 가동 (Port: 9201)
./peer_server 9201 seed1_chunks

# [창 3] Seed 2 서버 가동 (Port: 9202)
./peer_server 9202 seed2_chunks
```
**Step 3: 네트워크 등록 및 병렬 다운로드 시연**
새로운 [창 4]를 열어 아래 명령어를 순차적으로 실행합니다. 트래커에 Seed들을 등록하고, 다중 피어로부터 데이터를 동시에 긁어옵니다.

```Bash
# 1. Tracker에 Seed들의 파일 및 Chunk 보유 상태 등록
# (총 6개의 청크 중 각 Seed가 가진 번호 매핑)
./register_peer 127.0.0.1 7200 9201 source.bin 6 0,1,2
./register_peer 127.0.0.1 7200 9202 source.bin 6 3,4,5

# 2. 병렬 다운로드 실행
# (실행 시 pthread를 통해 다중 소켓 동시 다운로드 로그 출력)
./downloader 127.0.0.1 7200 source_chunks/manifest.txt source.bin downloaded_multi

# 3. 다운로드 완료된 파일 조립
./chunk_tool merge downloaded_multi/manifest.txt rebuilt.bin

# 4. 최종 무결성 검증
./verify_file source.bin rebuilt.bin
(검증이 정상적으로 완료될 경우 터미널에 SHA256 MATCH 메시지가 출력됩니다.)
```
## 성능 분석
- 측정 지표 : 멀티스레딩 기반 다중 Peer 연결 시의 Throughput(MB/s) 및 Elapsed Time(sec)
- 결과 : 다운로더가 여러 Peer로부터 데이터를 비동기적으로 동시에 수신함으로써 단일 TCP세션 대비 획기적인 대역폭 확장 및 다운로드 시간 단축을 증명함. 다운로드 완료시 내부적으로 gettimeofday() 를 호출하여 정확한 소요 시간을 로깅함.
