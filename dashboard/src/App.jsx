import React, { useState, useEffect } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { Activity, Zap, CheckCircle2 } from 'lucide-react';
import { generateClient } from 'aws-amplify/api';

// Amplify Gen2 GraphQL Client 생성 (amplify_outputs.json의 API Key 자동 사용)
const client = generateClient();

// Custom hook: AWS AppSync 실시간 구독 (DynamoDB ← Lambda ← IoT Core)
function useEfficiencyData() {
  const [dataHistory, setDataHistory] = useState([]);
  const [currentData, setCurrentData] = useState(null);

  useEffect(() => {
    // 1. 초기 데이터 로드: 페이지 접속 시 DynamoDB의 기존 데이터 최대 20개를 가져와 차트 초기화
    const fetchInitialData = async () => {
      try {
        const result = await client.models.EfficiencyData.list();
        const items = result.data ?? [];

        const sorted = items
          .sort((a, b) => a.timestamp - b.timestamp)
          .slice(-20)
          .map(item => ({
            ...item,
            v_in:       Number(item.v_in),
            i_in:       Number(item.i_in),
            p_in:       Number(item.p_in),
            v_out:      Number(item.v_out),
            i_out:      Number(item.i_out),
            p_out:      Number(item.p_out),
            efficiency: Number(item.efficiency),
            timeLabel:  new Date(item.timestamp * 1000)
                          .toLocaleTimeString('ko-KR', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
          }));

        if (sorted.length > 0) {
          setDataHistory(sorted);
          setCurrentData(sorted[sorted.length - 1]);
        }
      } catch (err) {
        console.error('초기 데이터 로드 오류:', err);
      }
    };

    fetchInitialData();

    // 2. 실시간 구독: Lambda가 DynamoDB에 새 항목을 저장할 때마다 자동으로 수신하여 차트 갱신
    const subscription = client.models.EfficiencyData.onCreate().subscribe({
      next: (item) => {
        const newData = {
          ...item,
          v_in:       Number(item.v_in),
          i_in:       Number(item.i_in),
          p_in:       Number(item.p_in),
          v_out:      Number(item.v_out),
          i_out:      Number(item.i_out),
          p_out:      Number(item.p_out),
          efficiency: Number(item.efficiency),
          timeLabel:  new Date(item.timestamp * 1000)
                        .toLocaleTimeString('ko-KR', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
        };

        setCurrentData(newData);
        setDataHistory(prev => {
          const next = [...prev, newData];
          return next.length > 20 ? next.slice(1) : next; // 최근 20개만 유지
        });
      },
      error: (err) => console.warn('Subscription 오류:', err)
    });

    // 컴포넌트 언마운트 시 구독 해제 (메모리 누수 방지)
    return () => subscription.unsubscribe();
  }, []);

  return { dataHistory, currentData };
}


function App() {
  const { dataHistory, currentData } = useEfficiencyData();

  if (!currentData) return <div className="flex h-screen items-center justify-center">Loading...</div>;

  return (
    <div className="min-h-screen bg-[#F9FAFB] flex justify-center py-8 px-4 font-sans text-[#111827]">
      <div className="w-full max-w-md space-y-6">
        
        {/* Header / Branding */}
        <header className="flex items-center justify-center space-x-3 mb-8">
          <img src="/HYU_logo_singlecolor.png" alt="Hanyang Logo" className="w-10 h-10 object-contain" />
          <h1 className="text-xl font-bold tracking-tight">벅 컨버터 효율 모니터링</h1>
        </header>

        {/* 1. Top Card Section - Current Efficiency */}
        <section className="bg-white rounded-3xl p-6 shadow-sm border border-gray-100 flex flex-col items-center relative">
          <div className="absolute top-6 right-6 flex items-center space-x-1.5 text-green-600 font-medium text-sm">
            <span className="relative flex h-2.5 w-2.5">
              <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-green-400 opacity-75"></span>
              <span className="relative inline-flex rounded-full h-2.5 w-2.5 bg-green-500"></span>
            </span>
            <span>{currentData.status === "Normal" ? "정상" : "경고"}</span>
          </div>

          <p className="text-gray-500 font-medium mb-1">현재 상태</p>
          <div className="flex items-baseline space-x-1 mb-4">
            <span className="text-5xl font-extrabold text-[#0E4A84] tracking-tight">
              {currentData.efficiency.toFixed(2)}
            </span>
            <span className="text-2xl font-bold text-[#0E4A84]">%</span>
          </div>
          
          <div className="flex w-full items-center justify-center space-x-6 text-sm text-gray-500">
            <div className="flex flex-col items-center">
              <span>입력: {currentData.p_in.toFixed(2)} W</span>
            </div>
            <div className="w-px h-4 bg-gray-200"></div>
            <div className="flex flex-col items-center">
              <span>출력: {currentData.p_out.toFixed(2)} W</span>
            </div>
          </div>
        </section>

        {/* 2. Center Chart Section */}
        <section className="bg-white rounded-3xl p-6 shadow-sm border border-gray-100">
          <div className="flex justify-between items-center mb-6">
            <h2 className="text-lg font-bold">실시간 효율 추이</h2>
            <div className="bg-blue-50 text-blue-500 p-1.5 rounded-lg">
              <Activity size={18} />
            </div>
          </div>
          <div className="h-48 w-full -ml-4">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={dataHistory}>
                <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="#E5E7EB" />
                <XAxis 
                  dataKey="timeLabel" 
                  tick={{fontSize: 12, fill: '#6B7280'}} 
                  axisLine={false} 
                  tickLine={false}
                  minTickGap={20}
                />
                <YAxis 
                  domain={[85, 100]} 
                  tickFormatter={(val) => `${val}%`}
                  tick={{fontSize: 12, fill: '#6B7280'}}
                  axisLine={false}
                  tickLine={false}
                  width={40}
                />
                <Tooltip 
                  contentStyle={{ borderRadius: '12px', border: 'none', boxShadow: '0 4px 6px -1px rgb(0 0 0 / 0.1)' }}
                  formatter={(value) => [`${Number(value).toFixed(2)}%`, '효율']}
                  labelStyle={{color: '#6B7280', marginBottom: '4px'}}
                />
                <Line 
                  type="monotone" 
                  dataKey="efficiency" 
                  stroke="#0E4A84" 
                  strokeWidth={3} 
                  dot={false}
                  activeDot={{ r: 6, fill: '#0E4A84', stroke: '#fff', strokeWidth: 2 }}
                  isAnimationActive={false} // Disable default animation for smoother real-time manual updates
                />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </section>

        {/* 3. Bottom Detail Data */}
        <section className="bg-[#F3F4F6] rounded-3xl p-6 shadow-inner border border-gray-100">
          <h2 className="text-lg font-bold mb-4">상세 측정값</h2>
          
          <div className="grid grid-cols-2 gap-4">
            {/* Input Side */}
            <div className="space-y-4">
              <div className="text-xs font-semibold text-gray-500 mb-2 text-center bg-gray-200 py-1 rounded-md">
                입력단 (INA260 #1)
              </div>
              
              <div className="bg-white p-3 rounded-2xl flex items-center space-x-3 shadow-sm">
                <div className="bg-blue-100 text-[#0E4A84] p-2 rounded-full font-bold text-xs w-8 h-8 flex items-center justify-center">
                  V
                </div>
                <div>
                  <div className="text-xs text-gray-500">전압 (V)</div>
                  <div className="font-bold text-lg">{currentData.v_in.toFixed(2)} V</div>
                </div>
              </div>

              <div className="bg-white p-3 rounded-2xl flex items-center space-x-3 shadow-sm">
                <div className="bg-blue-100 text-[#0E4A84] p-2 rounded-full font-bold text-xs w-8 h-8 flex items-center justify-center">
                  A
                </div>
                <div>
                  <div className="text-xs text-gray-500">전류 (A)</div>
                  <div className="font-bold text-lg">{currentData.i_in.toFixed(2)} A</div>
                </div>
              </div>
            </div>

            {/* Output Side */}
            <div className="space-y-4">
               <div className="text-xs font-semibold text-gray-500 mb-2 text-center bg-gray-200 py-1 rounded-md">
                출력단 (INA260 #2)
              </div>
              
              <div className="bg-white p-3 rounded-2xl flex items-center space-x-3 shadow-sm">
                <div className="bg-blue-100 text-[#0E4A84] p-2 rounded-full font-bold text-xs w-8 h-8 flex items-center justify-center">
                  V
                </div>
                <div>
                  <div className="text-xs text-gray-500">전압 (V)</div>
                  <div className="font-bold text-lg">{currentData.v_out.toFixed(2)} V</div>
                </div>
              </div>

              <div className="bg-white p-3 rounded-2xl flex items-center space-x-3 shadow-sm">
                <div className="bg-blue-100 text-[#0E4A84] p-2 rounded-full font-bold text-xs w-8 h-8 flex items-center justify-center">
                  A
                </div>
                <div>
                  <div className="text-xs text-gray-500">전류 (A)</div>
                  <div className="font-bold text-lg">{currentData.i_out.toFixed(2)} A</div>
                </div>
              </div>
            </div>
          </div>
        </section>

      </div>
    </div>
  );
}

export default App;
