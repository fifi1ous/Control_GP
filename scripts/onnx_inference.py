"""ONNX Runtime inference for the YOLO models exported from Ultralytics.

Replaces the ``ultralytics`` / ``torch`` dependency at runtime. Pre- and
post-processing mirror Ultralytics' predict pipeline (LetterBox + NMS for
detection, Resize + CenterCrop for classification) so the results match the
original ``.pt`` models.

Models are exported once with:
    yolo export model=segment.pt format=onnx opset=17 dynamic=True
    yolo export model=classification.pt format=onnx opset=17
"""

import ast

import cv2
import numpy as np
import onnxruntime as ort
from PIL import Image


def _imread(path):
    """Read an image as BGR; works with non-ASCII Windows paths (unlike cv2.imread)."""
    data = np.fromfile(path, dtype=np.uint8)
    img = cv2.imdecode(data, cv2.IMREAD_COLOR)
    if img is None:
        raise ValueError(f"Cannot read image: {path}")
    return img


class _OnnxModel:
    def __init__(self, model_path):
        self.session = ort.InferenceSession(model_path, providers=["CPUExecutionProvider"])
        self.input_name = self.session.get_inputs()[0].name

        meta = self.session.get_modelmeta().custom_metadata_map
        self.names = ast.literal_eval(meta["names"])  # {0: 'name', ...}
        self.imgsz = tuple(ast.literal_eval(meta["imgsz"]))  # (height, width)
        self.stride = int(meta.get("stride", 32))
        # exported with dynamic=True -> input height/width are symbolic, not ints
        self.dynamic = not all(isinstance(d, int) for d in self.session.get_inputs()[0].shape[2:])


class Detection:
    """Single detected box in original image pixel coordinates."""

    __slots__ = ("x1", "y1", "x2", "y2", "conf", "cls")

    def __init__(self, x1, y1, x2, y2, conf, cls):
        self.x1, self.y1, self.x2, self.y2 = x1, y1, x2, y2
        self.conf = conf
        self.cls = cls


class DetectionResult:
    def __init__(self, orig_shape, detections, names):
        self.orig_shape = orig_shape  # (height, width), same as Ultralytics
        self.detections = detections
        self.names = names


class OnnxDetector(_OnnxModel):
    """YOLO detection model. ``detector(image_path)`` -> DetectionResult."""

    def __init__(self, model_path, conf=0.25, iou=0.7, max_det=300):
        super().__init__(model_path)
        self.conf = conf
        self.iou = iou
        self.max_det = max_det

    def __call__(self, image_path):
        img = _imread(image_path)
        blob, ratio, (pad_w, pad_h) = self._letterbox(img)
        preds = self.session.run(None, {self.input_name: blob})[0]  # (1, 4 + nc, N)
        detections = self._postprocess(preds[0], ratio, pad_w, pad_h, img.shape[:2])
        return DetectionResult(img.shape[:2], detections, self.names)

    def _letterbox(self, img):
        h0, w0 = img.shape[:2]
        new_h, new_w = self.imgsz
        r = min(new_h / h0, new_w / w0)
        unpad_w, unpad_h = round(w0 * r), round(h0 * r)
        dw, dh = new_w - unpad_w, new_h - unpad_h
        if self.dynamic:
            # minimal padding to a multiple of stride, like Ultralytics does for .pt models
            dw, dh = dw % self.stride, dh % self.stride
        dw, dh = dw / 2, dh / 2

        if (w0, h0) != (unpad_w, unpad_h):
            img = cv2.resize(img, (unpad_w, unpad_h), interpolation=cv2.INTER_LINEAR)
        top, bottom = round(dh - 0.1), round(dh + 0.1)
        left, right = round(dw - 0.1), round(dw + 0.1)
        img = cv2.copyMakeBorder(img, top, bottom, left, right, cv2.BORDER_CONSTANT,
                                 value=(114, 114, 114))

        blob = img[:, :, ::-1].transpose(2, 0, 1)  # BGR HWC -> RGB CHW
        blob = np.ascontiguousarray(blob, dtype=np.float32)[None] / 255.0
        return blob, r, (left, top)

    def _postprocess(self, pred, ratio, pad_w, pad_h, orig_shape):
        nc = len(self.names)
        pred = pred.T  # (N, 4 + nc)
        scores = pred[:, 4:4 + nc]
        class_ids = scores.argmax(axis=1)
        confs = scores[np.arange(len(scores)), class_ids]

        keep = confs > self.conf
        if not keep.any():
            return []
        boxes, confs, class_ids = pred[keep, :4], confs[keep], class_ids[keep]

        # xywh -> xyxy
        xyxy = np.empty_like(boxes)
        xyxy[:, 0] = boxes[:, 0] - boxes[:, 2] / 2
        xyxy[:, 1] = boxes[:, 1] - boxes[:, 3] / 2
        xyxy[:, 2] = boxes[:, 0] + boxes[:, 2] / 2
        xyxy[:, 3] = boxes[:, 1] + boxes[:, 3] / 2

        # per-class NMS: offset boxes by class so different classes never overlap
        offset = class_ids[:, None].astype(np.float32) * 7680
        idx = _nms(xyxy + offset, confs, self.iou)[:self.max_det]

        # map back to original image pixels
        h0, w0 = orig_shape
        xyxy = xyxy[idx]
        xyxy[:, [0, 2]] = ((xyxy[:, [0, 2]] - pad_w) / ratio).clip(0, w0)
        xyxy[:, [1, 3]] = ((xyxy[:, [1, 3]] - pad_h) / ratio).clip(0, h0)

        return [
            Detection(float(b[0]), float(b[1]), float(b[2]), float(b[3]), float(c), int(k))
            for b, c, k in zip(xyxy, confs[idx], class_ids[idx])
        ]


def _nms(boxes, scores, iou_threshold):
    order = scores.argsort()[::-1]
    areas = (boxes[:, 2] - boxes[:, 0]) * (boxes[:, 3] - boxes[:, 1])
    keep = []
    while order.size > 0:
        i = order[0]
        keep.append(i)
        rest = order[1:]
        xx1 = np.maximum(boxes[i, 0], boxes[rest, 0])
        yy1 = np.maximum(boxes[i, 1], boxes[rest, 1])
        xx2 = np.minimum(boxes[i, 2], boxes[rest, 2])
        yy2 = np.minimum(boxes[i, 3], boxes[rest, 3])
        inter = np.clip(xx2 - xx1, 0, None) * np.clip(yy2 - yy1, 0, None)
        iou = inter / (areas[i] + areas[rest] - inter + 1e-9)
        order = rest[iou <= iou_threshold]
    return np.array(keep, dtype=np.int64)


class ClassificationResult:
    def __init__(self, probs, names):
        self.probs = probs
        self.top1 = int(probs.argmax())
        self.top1_name = names[self.top1]
        self.top1_conf = float(probs[self.top1])


class OnnxClassifier(_OnnxModel):
    """YOLO classification model. ``classifier(image_path)`` -> ClassificationResult."""

    def __call__(self, image_path):
        img = _imread(image_path)
        blob = self._preprocess(img)
        probs = self.session.run(None, {self.input_name: blob})[0][0]  # softmax is in the graph
        return ClassificationResult(probs, self.names)

    def _preprocess(self, img):
        # Same as Ultralytics classify_transforms: Resize(shortest edge) + CenterCrop
        size = self.imgsz[0]
        pil = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
        w, h = pil.size
        if w <= h:
            new_w, new_h = size, int(size * h / w)
        else:
            new_w, new_h = int(size * w / h), size
        pil = pil.resize((new_w, new_h), Image.BILINEAR)

        top = int(round((new_h - size) / 2.0))
        left = int(round((new_w - size) / 2.0))
        pil = pil.crop((left, top, left + size, top + size))

        blob = np.asarray(pil, dtype=np.float32).transpose(2, 0, 1) / 255.0
        return blob[None]
