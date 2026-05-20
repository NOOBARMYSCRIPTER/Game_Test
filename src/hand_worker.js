import { HandLandmarker, FilesetResolver } from "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@0.10.8/vision_bundle.js";

let handLandmarker = null;

async function initTracker() {
    try {
        console.log("[Worker] Загрузка ассетов MediaPipe v2...");
        
        const vision = await FilesetResolver.forVisionTasks(
            "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@0.10.8/wasm"
        );
        
        handLandmarker = await HandLandmarker.createFromOptions(vision, {
            baseOptions: {
                modelAssetPath: "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker_bundle/float16/1/hand_landmarker_bundle.task",
                delegate: "GPU"
            },
            runningMode: "IMAGE",
            numHands: 1
        });
        
        console.log("[Worker] Модель успешно инициализирована!");
        self.postMessage({ type: 'TRACKER_READY' });

    } catch (error) {
        console.error("[Worker] Критическая ошибка инициализации трекера:", error);
    }
}

initTracker();

self.onmessage = async (e) => {
    if (e.data.type === 'PROCESS_FRAME') {
        if (!handLandmarker) {
            self.postMessage({ type: 'RESULTS', count: 0 });
            return;
        }

        const imgBitmap = e.data.img;
        
        try {
            const results = handLandmarker.detect(imgBitmap);
            imgBitmap.close();

            let fingersCount = 0;

            if (results.landmarks && results.landmarks.length > 0) {
                const landmarks = results.landmarks[0];
                const fingerTips = [8, 12, 16, 20];
                
                fingerTips.forEach(tip => {
                    if (landmarks[tip].y < landmarks[tip - 3].y) {
                        fingersCount++;
                    }
                });

                if (results.handedness && results.handedness.length > 0) {
                    const isLeftHand = results.handedness[0][0].categoryName === 'Left';
                    if (isLeftHand) {
                        if (landmarks[4].x > landmarks[3].x) fingersCount++;
                    } else {
                        if (landmarks[4].x < landmarks[3].x) fingersCount++;
                    }
                }
            }

            self.postMessage({ type: 'RESULTS', count: fingersCount });

        } catch (err) {
            imgBitmap.close();
            self.postMessage({ type: 'RESULTS', count: 0 });
        }
    }
};
