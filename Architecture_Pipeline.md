# 벅 컨버터 효율 모니터링 시스템 (Architecture & Pipeline)

## 1. 시스템 개요
본 프로젝트는 ESP32와 INA260 센서를 이용하여 벅 컨버터(Buck Converter)의 입/출력 전압, 전류, 전력을 측정하고 실시간 효율을 계산하여 AWS 클라우드를 통해 웹 대시보드에 표출하는 IoT 모니터링 시스템입니다.

---

## 2. 전체 데이터 파이프라인
데이터는 하드웨어 센서 측정부터 웹 브라우저 표출까지 다음의 순서로 흐릅니다.

`ESP32 (하드웨어)` ➡️ `AWS IoT Core (MQTT)` ➡️ `AWS Lambda` ➡️ `AWS AppSync (GraphQL)` ➡️ `Amazon DynamoDB` & `React (프론트엔드)`

### 단계별 상세 동작

#### 1) 센서 데이터 수집 및 전송 (ESP32-S3 + INA260)
* **센서 측정**: ESP32가 2개의 INA260 전력 센서(입력측/출력측)와 I2C 통신하여 전압, 전류, 전력을 측정합니다.
* **효율 계산**: 측정된 전력 값을 바탕으로 실시간 변환 효율(%)을 계산합니다.
* **AWS IoT 전송**: X.509 인증서를 통해 AWS IoT Core에 보안 접속(TLS)을 맺고, `sensor/buck/data` 토픽으로 측정 데이터를 JSON 형태로 Publish(게시) 합니다.

#### 2) 메시지 라우팅 및 데이터 변환 (AWS IoT Core & Lambda)
* **IoT Rule**: AWS IoT Core는 `sensor/buck/data` 토픽으로 들어오는 메시지를 감지하는 규칙(Rule)을 실행합니다. (`SELECT * FROM 'sensor/buck/data'`)
* **AWS Lambda**: IoT Rule에 의해 트리거된 Lambda 함수(`IoTToDynamoDB`)가 실행됩니다.
  * 들어온 페이로드에 서버의 '현재 시간(timestamp)'과 UUID를 주입하여 데이터의 무결성을 보장합니다.
  * 데이터를 DynamoDB에 직접 쓰지 않고, 프론트엔드의 **실시간 구독(Subscription) 이벤트를 강제로 발생시키기 위해** AWS AppSync GraphQL Mutation API를 HTTP POST로 호출합니다.

#### 3) 실시간 동기화 및 DB 저장 (AWS AppSync & DynamoDB)
* **AWS AppSync (Amplify Gen2)**: Lambda로부터 GraphQL Mutation(`createEfficiencyData`) 요청을 수신합니다.
  * AppSync는 받은 데이터를 자동으로 연결된 **Amazon DynamoDB** 테이블에 영구 저장합니다.
  * 동시에, 해당 데이터 변경 이벤트를 구독(Subscribe)하고 있는 **모든 프론트엔드 클라이언트들에게 WebSocket 채널을 통해 실시간으로 브로드캐스트(푸시 알림)** 합니다.

#### 4) 실시간 대시보드 표출 (React + Vite)
* **초기 로드**: 사용자가 웹페이지에 접속하면 AppSync Query(`list`)를 호출하여 최근 20개의 데이터를 DynamoDB에서 가져와 초기 차트를 그립니다.
* **실시간 갱신**: AppSync Subscription(`onCreate`)을 통해 새로운 데이터가 추가될 때마다 백그라운드에서 데이터를 밀어내어(Push) 수신받고, React 상태(`currentData`, `dataHistory`)를 즉시 업데이트하여 차트(Recharts)와 계기판을 **새로고침 없이 실시간으로 갱신**합니다.

---

## 3. 사용된 기술 스택
* **Hardware**: ESP32-S3, INA260 (I2C Power Sensor)
* **Firmware**: Arduino C/C++ (PubSubClient, ArduinoJson)
* **AWS Backend**: AWS IoT Core, AWS Lambda (Python 3.12), AWS AppSync (GraphQL), Amazon DynamoDB
* **Frontend**: React 18, Vite, TailwindCSS, Recharts, AWS Amplify Gen2
* **Hosting/CI-CD**: AWS Amplify Hosting, GitHub Actions

---

## 4. 보안 및 인증 흐름
* **하드웨어 ↔ AWS**: X.509 기기 인증서(Device Certificate) 및 AWS IoT Policy를 통한 상호 인증 (mTLS)
* **Lambda ↔ AppSync**: AppSync API Key를 사용한 시스템 내부 인증
* **Frontend ↔ AppSync**: AppSync API Key (Amplify Gen2 자동 구성 파일 `amplify_outputs.json` 기반)

---

## 5. 관리 및 주의사항
1. **IoT Policy**: ESP32가 AWS IoT Core에 접속하려면, 반드시 기기 인증서에 `iot:Connect`와 `iot:Publish` 권한이 허용된 정책(Policy)이 연결(Attach)되어 있어야 합니다.
2. **Lambda 구조**: Lambda에서 DynamoDB Boto3 `put_item`으로 직접 쓸 경우 AppSync Subscription이 발동하지 않아 프론트엔드 화면이 실시간으로 변하지 않습니다. 반드시 현재 설정된 GraphQL Mutation 방식을 유지해야 합니다.
3. **API Key 갱신**: AppSync API Key는 만료 기한이 존재합니다. 만료 시 Amplify를 통해 API Key를 갱신하거나 재배포를 수행해야 합니다.
