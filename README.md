Buck Converter Efficiency Real-Time Measurement System
설계 기준 문서 (Design Reference Document)
버전: v1.0 | 작성일: 2026-05-12 | 담당: cylee
목적: ESP32 + INA260 기반 DC-DC 벅 컨버터 효율 실시간 측정 및 AWS 클라우드 시각화

목차
시스템 개요
하드웨어 구성 및 측정 원리
시스템 아키텍처 및 데이터 흐름
Phase A: 엣지(Edge) 단계
Phase B: 클라우드(Cloud) 단계
Phase C: 프론트엔드(Frontend) 단계
핵심 성공 요인 (CSF)
1. 시스템 개요
본 프로젝트는 **DC-DC 벅 컨버터(Buck Converter)**의 전력 변환 효율(
η
)을 실시간으로 측정하고, 이를 AWS 클라우드 파이프라인을 통해 웹 대시보드에 시각화하는 IoT 시스템입니다.

항목	내용
측정 대상	DC-DC 벅 컨버터 전력 변환 효율
엣지 디바이스	ESP32 (Wi-Fi 내장 MCU)
센서	INA260 × 2 (전압/전류 복합 센서)
통신 프로토콜	I2C (센서↔ESP32), MQTT (ESP32↔AWS)
클라우드	AWS IoT Core / Lambda / DynamoDB / Amplify
프론트엔드	React 또는 Vue.js + Recharts
2. 하드웨어 구성 및 측정 원리
2.1 효율 계산 공식
벅 컨버터의 효율은 출력 전력 대비 입력 전력의 비율로 정의됩니다.

η
=
(
P
o
u
t
P
i
n
)
×
100
=
(
V
o
u
t
×
I
o
u
t
V
i
n
×
I
i
n
)
×
100
 
[

2.2 INA260 센서 배치
INA260은 전압(V)과 전류(A)를 동시에 측정하는 통합 센서로, 입력단과 출력단에 각각 1개씩 배치합니다.

┌─────────────────────────────────────────────────────────────────────────┐
│                           회로 측정 구성도                                │
│                                                                         │
│  [PSU] ── [INA260 #1] ── [Buck Converter] ── [INA260 #2] ── [Load]     │
│             (Input)           (변환)             (Output)               │
│           0x40 addr                             0x41 addr               │
│           V_in, I_in                            V_out, I_out            │
│                 │                                    │                  │
│                 └──────────── I2C Bus ───────────────┘                  │
│                                   │                                     │
│                                [ESP32]                                  │
└─────────────────────────────────────────────────────────────────────────┘
측정 지점	센서	I2C 주소	측정값	연산 결과
Input Side	INA260 #1	0x40 (A0=GND, A1=GND)	
V
i
n
, 
I
i
n
P
i
n
=
V
i
n
×
I
i
n
Output Side	INA260 #2	0x41 (A0=VCC, A1=GND)	
V
o
u
t
, 
I
o
u
t
P
o
u
t
=
V
o
u
t
×
I
o
u
t
2.3 I2C 주소 설정 (A0/A1 핀)
INA260의 I2C 주소는 A0, A1 핀의 전위 설정으로 결정됩니다.

A1	A0	주소	용도
GND	GND	0x40	Input 측정 센서
GND	VCC	0x41	Output 측정 센서
⚠️ 주의: 두 INA260은 동일한 I2C 버스(SDA/SCL)에 연결되므로, 반드시 서로 다른 주소를 할당해야 충돌을 방지할 수 있습니다.

3. 시스템 아키텍처 및 데이터 흐름
전체 데이터 파이프라인은 Edge → Cloud → Frontend 3단계로 구성됩니다.

[INA260 x2]
     │ I2C
     ▼
 [ESP32]  ── Wi-Fi / MQTT ──►  [AWS IoT Core]
  (연산)                              │ Rule Engine
                                      ▼
                               [AWS Lambda]
                                (데이터 가공)
                                      │
                                      ▼
                              [Amazon DynamoDB]
                             (Timestamp PK 저장)
                                      │
                          ┌───────────┘
                          ▼
               [AppSync / API Gateway]
                          │
                          ▼
            [React/Vue.js @ AWS Amplify]
                  (실시간 대시보드)
Phase A: 엣지(Edge) 단계 — ESP32
단계	내용
① 데이터 수집	I2C 통신으로 INA260 #1(0x40), #2(0x41)에서 전압·전류 읽기
② 연산	
η
=
V
o
u
t
×
I
o
u
t
V
i
n
×
I
i
n
×
100
 계산
③ 전송	Wi-Fi 연결 후 MQTT 프로토콜로 AWS IoT Core에 JSON 페이로드 발행
MQTT 페이로드 예시:

{
  "timestamp": 1747006000,
  "v_in": 12.05,
  "i_in": 1.32,
  "p_in": 15.91,
  "v_out": 5.02,
  "i_out": 2.98,
  "p_out": 14.96,
  "efficiency": 94.03
}
Phase B: 클라우드(Cloud) 단계 — AWS
서비스	역할
AWS IoT Core	MQTT 브로커. Rule Engine이 특정 Topic을 구독하여 Lambda 트리거
AWS Lambda	Rule에 의해 호출됨. 데이터 유효성 검사 및 변환 후 DynamoDB 저장
Amazon DynamoDB	NoSQL DB. timestamp를 파티션 키(Partition Key)로 사용하여 시계열 데이터 적재
DynamoDB 테이블 스키마 (예시):

필드	타입	역할
timestamp	Number (PK)	파티션 키 (Unix Epoch)
v_in	Number	입력 전압 [V]
i_in	Number	입력 전류 [A]
p_in	Number	입력 전력 [W]
v_out	Number	출력 전압 [V]
i_out	Number	출력 전류 [A]
p_out	Number	출력 전력 [W]
efficiency	Number	효율 [%]
Phase C: 프론트엔드(Frontend) 단계 — AWS Amplify
구성 요소	역할
AppSync / API Gateway	DynamoDB 데이터를 GraphQL 또는 REST API로 프론트에 노출
React / Vue.js	웹 대시보드 UI 구성
Recharts (또는 Chart.js)	실시간 효율 변화, 전압/전류 추이 시각화
AWS Amplify Hosting	정적 웹 앱 배포 및 CI/CD 자동화
4. 핵심 성공 요인 (Critical Success Factors)
CSF #1 — I2C 주소 및 배선의 정확성 (Hardware Reliability)
시스템의 모든 연산은 두 INA260 센서에서 올바른 데이터가 수집되는 것을 전제로 합니다. A0/A1 핀 설정 오류나 SDA/SCL 배선 불량은 측정값 오류 또는 I2C 버스 충돌로 이어져, 이후 전체 파이프라인의 데이터 신뢰성을 붕괴시킵니다. **실측 교정(Calibration)**과 배선 검증이 최우선 과제입니다.

CSF #2 — AWS IoT 인증 및 보안 설정 (Cloud Security & Connectivity)
ESP32가 AWS IoT Core에 연결하려면 X.509 인증서 기반 TLS 상호 인증이 필수입니다. 인증서 관리, IoT Policy, 올바른 MQTT Topic 구조 설계가 잘못될 경우 클라우드 연결 자체가 불가능해집니다. 또한 Lambda 실행 역할(IAM Role)의 DynamoDB 쓰기 권한 설정이 누락되면 데이터가 유실됩니다. **IAM 권한 최소 원칙(PoLP)**을 지키며 단계별로 연결을 검증해야 합니다.

CSF #3 — 실시간성과 데이터 레이턴시 관리 (End-to-End Latency)
웹 대시보드의 가치는 얼마나 빠르게 측정 결과를 반영하느냐에 달려 있습니다. ESP32 샘플링 주기, MQTT QoS 설정, Lambda 콜드 스타트, DynamoDB 읽기 용량, 그리고 프론트엔드 폴링 주기가 누적되어 전체 레이턴시를 결정합니다. 각 구간의 지연을 측정하고 허용 가능한 레이턴시 목표를 수립한 뒤, AppSync의 실시간 구독(Subscription) 기능 활용을 검토하는 것이 권장됩니다.

본 문서는 프로젝트 진행에 따라 지속적으로 업데이트됩니다.
