# Frontend Integration Guide — Student Assignment Priority Manager

This guide explains how frontend applications (React, Vue, Svelte, Angular, iOS/Android, or vanilla TypeScript/JavaScript) should integrate with the Student Assignment Priority Manager API.

---

## 1. Core Architectural Principle

> [!IMPORTANT]
> **Never sort assignments or calculate priority on the client side.**  
> The C engine is the single authoritative source of truth for priority calculations, calendar date logic, and tie-breaking. Client-side re-sorting leads to divergent schedules and invalidates the priority queue guarantees.

---

## 2. API Base URL & Headers

```typescript
const API_BASE_URL = "http://127.0.0.1:5000/api/v1";

const defaultHeaders = {
  "Content-Type": "application/json",
  "Accept": "application/json",
};
```

---

## 3. Key UI Patterns & Code Examples

### Pattern A: "Smart Focus / Next Task" Card
The Focus card presents the single most important task the student should work on right now.

```typescript
// 1. Peek at the top task without completing it
async function getNextFocusTask() {
  const res = await fetch(`${API_BASE_URL}/statistics`);
  if (!res.ok) throw new Error("Failed to fetch statistics");
  const data = await res.json();
  return data.data.next_highest_assignment; // returns null if queue is empty
}

// 2. Complete and extract the top task (triggers micro-animation in UI)
async function completeFocusTask() {
  const res = await fetch(`${API_BASE_URL}/assignments/process-next`, {
    method: "POST",
    headers: defaultHeaders,
  });

  const payload = await res.json();
  if (!res.ok) {
    if (payload.code === "QUEUE_EMPTY") {
      alert("All assignments are completed! Queue is empty.");
      return null;
    }
    throw new Error(payload.message || "Failed to process task");
  }

  // payload.data is the completed assignment record with status: "COMPLETED"
  return payload.data;
}
```

---

### Pattern B: Assignments List / Priority Matrix View
Allows viewing all pending coursework, sorted in authoritative priority order.

```typescript
// Fetch pending assignments ordered strictly by C priority engine
async function fetchPriorityOrderedAssignments() {
  const res = await fetch(`${API_BASE_URL}/assignments?sorted=true`);
  if (!res.ok) throw new Error("Failed to fetch assignments");
  const data = await res.json();
  return data.data; // Array of Assignment objects in descending priority order
}
```

---

### Pattern C: Adding Coursework with Validation
Submitting a new assignment form.

```typescript
interface CreateAssignmentDTO {
  id: number;
  title: string;
  subject: string;
  deadline: string; // YYYY-MM-DD
  priority: "LOW" | "MEDIUM" | "HIGH" | "URGENT";
}

async function addAssignment(assignment: CreateAssignmentDTO) {
  const res = await fetch(`${API_BASE_URL}/assignments`, {
    method: "POST",
    headers: defaultHeaders,
    body: JSON.stringify(assignment),
  });

  const payload = await res.json();
  if (!res.ok) {
    if (payload.code === "DUPLICATE_ID") {
      throw new Error(`Assignment ID ${assignment.id} already exists.`);
    }
    if (payload.code === "QUEUE_FULL") {
      throw new Error("Cannot add assignment: queue capacity (100) reached.");
    }
    if (payload.code === "VALIDATION_ERROR") {
      throw new Error(`Validation failed: ${payload.details?.join(", ") || payload.message}`);
    }
    throw new Error(payload.message || "Unknown error creating assignment");
  }

  return payload.data;
}
```

---

### Pattern D: Workload & Capacity Gauge
Render progress bars, completion meters, and priority distribution badges.

```typescript
async function fetchDashboardMetrics() {
  const res = await fetch(`${API_BASE_URL}/statistics`);
  if (!res.ok) throw new Error("Failed to fetch stats");
  const { data } = await res.json();

  return {
    totalPending: data.total_pending,
    totalCompleted: data.total_completed,
    capacityLimit: data.capacity, // 100
    capacityUsedPct: data.capacity_used_percentage,
    urgentCount: data.priority_breakdown.URGENT,
    highCount: data.priority_breakdown.HIGH,
    mediumCount: data.priority_breakdown.MEDIUM,
    lowCount: data.priority_breakdown.LOW,
  };
}
```

---

### Pattern E: Completed History Feed
Displaying historical record of completed homework.

```typescript
async function fetchCompletionHistory() {
  const res = await fetch(`${API_BASE_URL}/history`);
  if (!res.ok) throw new Error("Failed to fetch history");
  const data = await res.json();
  return data.data; // List of completed assignments
}
```

---

## 4. Frontend Error Handling Checklist

| Error Code | Meaning | Recommended UI Reaction |
|---|---|---|
| `VALIDATION_ERROR` | Date or priority or title format invalid | Highlight the specific input fields listed in `details`. |
| `DUPLICATE_ID` | Assignment ID already in use | Prompt user to choose another ID or auto-generate `max(id) + 1`. |
| `QUEUE_FULL` | 100 pending assignments max limit | Display modal warning that 100 assignments are already active. |
| `QUEUE_EMPTY` | Popping empty queue | Disable the "Process Next" button when `total_pending === 0`. |
| `NOT_FOUND` | Resource was deleted or does not exist | Refresh list and show notification that item is no longer pending. |
| `STORAGE_ERROR` | In-memory rollback triggered on disk failure | Inform user that changes could not be saved to disk and rolled back. |
