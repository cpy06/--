/*
 * 程序名称：基于单向链表的学生成绩单管理系统
 * 编程语言：C 语言
 * 功能说明：
 * 1. 使用单向链表管理学生成绩单；
 * 2. 支持文本文件读写、增删改查、批量操作、排序和撤销；
 * 3. 每条记录包含学号、姓名、5 门课程成绩、总分和平均分。
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_LEN 16
#define NAME_LEN 32
#define COURSE_COUNT 5
#define PATH_LEN 260
#define INPUT_LEN 512
#define UNDO_LIMIT 20

typedef struct Student {
    char id[ID_LEN];
    char name[NAME_LEN];
    float scores[COURSE_COUNT];
    float total;
    float average;
    struct Student *next;
} Student;

typedef struct {
    Student *head;
    Student *tail;
    int size;
} List;

typedef struct Snapshot {
    List data;
    char action[64];
    struct Snapshot *next;
} Snapshot;

typedef struct {
    Snapshot *top;
    int size;
} UndoStack;

typedef struct {
    int loaded_count;
    int duplicate_count;
    int invalid_count;
} LoadReport;

static const char *COURSE_NAMES[COURSE_COUNT] = {
    "语文", "数学", "英语", "C语言", "数据结构"
};

/* 去掉字符串首尾空白字符，便于统一处理用户输入。 */
static char *trim(char *text) {
    char *end;
    while (*text && isspace((unsigned char)*text)) {
        text++;
    }
    if (*text == '\0') {
        return text;
    }
    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return text;
}

static int read_line(char *buffer, size_t length) {
    if (fgets(buffer, (int)length, stdin) == NULL) {
        return 0;
    }
    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 1;
}

static int read_nonempty_line(const char *prompt, char *buffer, size_t length) {
    char input[INPUT_LEN];
    while (1) {
        if (prompt != NULL) {
            printf("%s", prompt);
        }
        if (!read_line(input, sizeof(input))) {
            return 0;
        }
        {
            char *value = trim(input);
            if (*value == '\0') {
                printf("输入不能为空，请重新输入。\n");
                continue;
            }
            if (strlen(value) >= length) {
                printf("输入过长，最多允许 %lu 个字符。\n", (unsigned long)(length - 1));
                continue;
            }
            strcpy(buffer, value);
            return 1;
        }
    }
}

static int read_int_range(const char *prompt, int min, int max, int *value) {
    char input[INPUT_LEN];
    while (1) {
        char *end = NULL;
        long number;
        if (prompt != NULL) {
            printf("%s", prompt);
        }
        if (!read_line(input, sizeof(input))) {
            return 0;
        }
        {
            char *text = trim(input);
            if (*text == '\0') {
                printf("输入不能为空，请重新输入。\n");
                continue;
            }
            number = strtol(text, &end, 10);
            end = trim(end);
            if (end == text || *end != '\0') {
                printf("请输入有效的整数。\n");
                continue;
            }
            if (number < min || number > max) {
                printf("输入超出范围，应在 %d 到 %d 之间。\n", min, max);
                continue;
            }
            *value = (int)number;
            return 1;
        }
    }
}

static int read_float_range(const char *prompt, float min, float max, float *value) {
    char input[INPUT_LEN];
    while (1) {
        char *end = NULL;
        double number;
        if (prompt != NULL) {
            printf("%s", prompt);
        }
        if (!read_line(input, sizeof(input))) {
            return 0;
        }
        {
            char *text = trim(input);
            if (*text == '\0') {
                printf("输入不能为空，请重新输入。\n");
                continue;
            }
            number = strtod(text, &end);
            end = trim(end);
            if (end == text || *end != '\0') {
                printf("请输入有效的数字。\n");
                continue;
            }
            if (number < min || number > max) {
                printf("输入超出范围，应在 %.1f 到 %.1f 之间。\n", min, max);
                continue;
            }
            *value = (float)number;
            return 1;
        }
    }
}

static int read_yes_no(const char *prompt, int *answer) {
    char input[INPUT_LEN];
    while (1) {
        if (prompt != NULL) {
            printf("%s", prompt);
        }
        if (!read_line(input, sizeof(input))) {
            return 0;
        }
        {
            char *text = trim(input);
            if (text[0] == 'y' || text[0] == 'Y') {
                *answer = 1;
                return 1;
            }
            if (text[0] == 'n' || text[0] == 'N') {
                *answer = 0;
                return 1;
            }
            printf("请输入 y 或 n。\n");
        }
    }
}

static void calculate_scores(Student *student) {
    int i;
    float sum = 0.0f;
    for (i = 0; i < COURSE_COUNT; i++) {
        sum += student->scores[i];
    }
    student->total = sum;
    student->average = sum / COURSE_COUNT;
}

static void init_list(List *list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

static Student *create_student(const char *id, const char *name, const float scores[COURSE_COUNT]) {
    Student *student = (Student *)malloc(sizeof(Student));
    int i;
    if (student == NULL) {
        return NULL;
    }
    strncpy(student->id, id, ID_LEN - 1);
    student->id[ID_LEN - 1] = '\0';
    strncpy(student->name, name, NAME_LEN - 1);
    student->name[NAME_LEN - 1] = '\0';
    for (i = 0; i < COURSE_COUNT; i++) {
        student->scores[i] = scores[i];
    }
    calculate_scores(student);
    student->next = NULL;
    return student;
}

static void free_list(List *list) {
    Student *current = list->head;
    while (current != NULL) {
        Student *next = current->next;
        free(current);
        current = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

static Student *find_student_by_id(List *list, const char *id) {
    Student *current = list->head;
    while (current != NULL) {
        if (strcmp(current->id, id) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static void append_student(List *list, Student *student) {
    if (list->head == NULL) {
        list->head = student;
        list->tail = student;
        list->size++;
        return;
    }
    list->tail->next = student;
    list->tail = student;
    list->size++;
}

static int delete_student_by_id(List *list, const char *id) {
    Student *current = list->head;
    Student *previous = NULL;
    while (current != NULL) {
        if (strcmp(current->id, id) == 0) {
            if (previous == NULL) {
                list->head = current->next;
            } else {
                previous->next = current->next;
            }
            if (current == list->tail) {
                list->tail = previous;
            }
            free(current);
            list->size--;
            return 1;
        }
        previous = current;
        current = current->next;
    }
    return 0;
}

static int copy_list(const List *source, List *target) {
    Student *current = source->head;
    init_list(target);
    while (current != NULL) {
        Student *copy = create_student(current->id, current->name, current->scores);
        if (copy == NULL) {
            free_list(target);
            return 0;
        }
        append_student(target, copy);
        current = current->next;
    }
    return 1;
}

static void init_undo_stack(UndoStack *stack) {
    stack->top = NULL;
    stack->size = 0;
}

static void free_undo_stack(UndoStack *stack) {
    Snapshot *current = stack->top;
    while (current != NULL) {
        Snapshot *next = current->next;
        free_list(&current->data);
        free(current);
        current = next;
    }
    stack->top = NULL;
    stack->size = 0;
}

static void trim_undo_stack(UndoStack *stack) {
    Snapshot *current;
    Snapshot *previous;
    if (stack->size <= UNDO_LIMIT) {
        return;
    }
    current = stack->top;
    previous = NULL;
    while (current != NULL && current->next != NULL) {
        previous = current;
        current = current->next;
    }
    if (previous != NULL && current != NULL) {
        previous->next = NULL;
        free_list(&current->data);
        free(current);
        stack->size--;
    }
}

static int push_undo_snapshot(UndoStack *stack, const List *list, const char *action) {
    Snapshot *snapshot = (Snapshot *)malloc(sizeof(Snapshot));
    if (snapshot == NULL) {
        return 0;
    }
    if (!copy_list(list, &snapshot->data)) {
        free(snapshot);
        return 0;
    }
    strncpy(snapshot->action, action, sizeof(snapshot->action) - 1);
    snapshot->action[sizeof(snapshot->action) - 1] = '\0';
    snapshot->next = stack->top;
    stack->top = snapshot;
    stack->size++;
    trim_undo_stack(stack);
    return 1;
}

static int undo_last_action(List *list, UndoStack *stack) {
    Snapshot *snapshot;
    if (stack->top == NULL) {
        return 0;
    }
    snapshot = stack->top;
    stack->top = snapshot->next;
    stack->size--;
    free_list(list);
    *list = snapshot->data;
    printf("已撤销上一步操作：%s。\n", snapshot->action);
    free(snapshot);
    return 1;
}

/* 校验路径，避免空路径、控制字符和常见非法通配符。 */
static int validate_path(const char *path) {
    size_t i;
    static const char *forbidden = "<>|\"?*";
    if (path == NULL || *path == '\0') {
        printf("路径不能为空。\n");
        return 0;
    }
    if (strlen(path) >= PATH_LEN) {
        printf("路径过长，最多允许 %d 个字符。\n", PATH_LEN - 1);
        return 0;
    }
    for (i = 0; path[i] != '\0'; i++) {
        if ((unsigned char)path[i] < 32) {
            printf("路径中包含非法控制字符。\n");
            return 0;
        }
        if (strchr(forbidden, path[i]) != NULL) {
            printf("路径中不能包含字符：< > | \" ? *\n");
            return 0;
        }
    }
    return 1;
}

static int parse_file_line(const char *line, char id[ID_LEN], char name[NAME_LEN], float scores[COURSE_COUNT]) {
    char extra[32];
    int count = sscanf(
        line,
        "%15s %31s %f %f %f %f %f %31s",
        id,
        name,
        &scores[0],
        &scores[1],
        &scores[2],
        &scores[3],
        &scores[4],
        extra
    );
    int i;
    if (count != 7) {
        return 0;
    }
    for (i = 0; i < COURSE_COUNT; i++) {
        if (scores[i] < 0.0f || scores[i] > 100.0f) {
            return 0;
        }
    }
    return 1;
}

/* 从文本文件载入成绩单，按“学号 姓名 5门成绩”格式解析。 */
static int load_from_file(List *list, const char *path, LoadReport *report) {
    FILE *file;
    char line[INPUT_LEN];
    List temp;
    init_list(&temp);

    report->loaded_count = 0;
    report->duplicate_count = 0;
    report->invalid_count = 0;

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char id[ID_LEN];
        char name[NAME_LEN];
        float scores[COURSE_COUNT];
        char *text = trim(line);
        if (*text == '\0') {
            continue;
        }
        if (!parse_file_line(text, id, name, scores)) {
            report->invalid_count++;
            continue;
        }
        if (find_student_by_id(&temp, id) != NULL) {
            report->duplicate_count++;
            continue;
        }
        {
            Student *student = create_student(id, name, scores);
            if (student == NULL) {
                fclose(file);
                free_list(&temp);
                return 0;
            }
            append_student(&temp, student);
            report->loaded_count++;
        }
    }

    fclose(file);
    free_list(list);
    *list = temp;
    return 1;
}

static int save_to_file(const List *list, const char *path) {
    FILE *file = fopen(path, "w");
    Student *current = list->head;
    int i;
    if (file == NULL) {
        return 0;
    }
    while (current != NULL) {
        fprintf(file, "%s %s", current->id, current->name);
        for (i = 0; i < COURSE_COUNT; i++) {
            fprintf(file, " %.1f", current->scores[i]);
        }
        fprintf(file, "\n");
        current = current->next;
    }
    fclose(file);
    return 1;
}

static void print_format_tip(void) {
    printf("文本文件格式说明：每行一条记录，格式为\n");
    printf("学号 姓名 语文 数学 英语 C语言 数据结构\n");
    printf("示例：2023001 张三 88 90 85 92 87\n");
}

static void print_header(void) {
    int i;
    printf("%-14s %-12s", "学号", "姓名");
    for (i = 0; i < COURSE_COUNT; i++) {
        printf("%-10s", COURSE_NAMES[i]);
    }
    printf("%-10s%-10s\n", "总分", "平均分");
    printf("-------------------------------------------------------------------------------\n");
}

static void print_student(const Student *student) {
    int i;
    printf("%-14s %-12s", student->id, student->name);
    for (i = 0; i < COURSE_COUNT; i++) {
        printf("%-10.1f", student->scores[i]);
    }
    printf("%-10.1f%-10.1f\n", student->total, student->average);
}

static void print_all_students(const List *list) {
    Student *current = list->head;
    if (current == NULL) {
        printf("当前没有学生记录。\n");
        return;
    }
    print_header();
    while (current != NULL) {
        print_student(current);
        current = current->next;
    }
    printf("当前共有 %d 条记录。\n", list->size);
}

static int input_student_info(char id[ID_LEN], char name[NAME_LEN], float scores[COURSE_COUNT]) {
    int i;
    if (!read_nonempty_line("请输入学号：", id, ID_LEN)) {
        return 0;
    }
    if (!read_nonempty_line("请输入姓名：", name, NAME_LEN)) {
        return 0;
    }
    for (i = 0; i < COURSE_COUNT; i++) {
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "请输入%s成绩（0-100）：", COURSE_NAMES[i]);
        if (!read_float_range(prompt, 0.0f, 100.0f, &scores[i])) {
            return 0;
        }
    }
    return 1;
}

static void add_one_student(List *list, UndoStack *stack) {
    char id[ID_LEN];
    char name[NAME_LEN];
    float scores[COURSE_COUNT];
    Student *student;

    if (!input_student_info(id, name, scores)) {
        return;
    }
    if (find_student_by_id(list, id) != NULL) {
        printf("学号已存在，不能重复添加。\n");
        return;
    }
    if (!push_undo_snapshot(stack, list, "添加单条记录")) {
        printf("保存撤销快照失败，本次操作取消。\n");
        return;
    }
    student = create_student(id, name, scores);
    if (student == NULL) {
        printf("内存分配失败。\n");
        undo_last_action(list, stack);
        return;
    }
    append_student(list, student);
    printf("学生记录添加成功。\n");
}

static void add_multiple_students(List *list, UndoStack *stack) {
    int count;
    int i;
    int success = 0;
    if (!read_int_range("请输入批量添加的人数（1-100）：", 1, 100, &count)) {
        return;
    }
    if (!push_undo_snapshot(stack, list, "批量添加记录")) {
        printf("保存撤销快照失败，本次操作取消。\n");
        return;
    }
    for (i = 0; i < count; i++) {
        char id[ID_LEN];
        char name[NAME_LEN];
        float scores[COURSE_COUNT];
        Student *student;
        printf("\n正在录入第 %d/%d 条记录：\n", i + 1, count);
        if (!input_student_info(id, name, scores)) {
            printf("输入中断，后续记录未继续添加。\n");
            break;
        }
        if (find_student_by_id(list, id) != NULL) {
            printf("学号 %s 已存在，本条记录已跳过。\n", id);
            continue;
        }
        student = create_student(id, name, scores);
        if (student == NULL) {
            printf("内存分配失败，批量添加提前结束。\n");
            break;
        }
        append_student(list, student);
        success++;
    }
    printf("批量添加完成，成功添加 %d 条记录。\n", success);
}

static void modify_student_info(List *list, UndoStack *stack) {
    char old_id[ID_LEN];
    char new_id[ID_LEN];
    int choice;
    int i;
    Student *student;

    if (!read_nonempty_line("请输入要修改的学号：", old_id, ID_LEN)) {
        return;
    }
    student = find_student_by_id(list, old_id);
    if (student == NULL) {
        printf("未找到该学号对应的记录。\n");
        return;
    }
    if (!push_undo_snapshot(stack, list, "修改记录")) {
        printf("保存撤销快照失败，本次操作取消。\n");
        return;
    }

    printf("请选择修改内容：\n");
    printf("1. 修改学号\n");
    printf("2. 修改姓名\n");
    printf("3. 修改五门成绩\n");
    printf("4. 同时修改学号、姓名和成绩\n");
    if (!read_int_range("请输入选项：", 1, 4, &choice)) {
        return;
    }

    if (choice == 1 || choice == 4) {
        if (!read_nonempty_line("请输入新的学号：", new_id, ID_LEN)) {
            return;
        }
        if (strcmp(new_id, old_id) != 0 && find_student_by_id(list, new_id) != NULL) {
            printf("新的学号已存在，修改失败。\n");
            undo_last_action(list, stack);
            return;
        }
        strcpy(student->id, new_id);
    }
    if (choice == 2 || choice == 4) {
        if (!read_nonempty_line("请输入新的姓名：", student->name, NAME_LEN)) {
            return;
        }
    }
    if (choice == 3 || choice == 4) {
        for (i = 0; i < COURSE_COUNT; i++) {
            char prompt[64];
            snprintf(prompt, sizeof(prompt), "请输入新的%s成绩（0-100）：", COURSE_NAMES[i]);
            if (!read_float_range(prompt, 0.0f, 100.0f, &student->scores[i])) {
                return;
            }
        }
        calculate_scores(student);
    }
    printf("学生记录修改成功。\n");
}

static void delete_one_student(List *list, UndoStack *stack) {
    char id[ID_LEN];
    if (!read_nonempty_line("请输入要删除的学号：", id, ID_LEN)) {
        return;
    }
    if (find_student_by_id(list, id) == NULL) {
        printf("未找到该学号对应的记录。\n");
        return;
    }
    if (!push_undo_snapshot(stack, list, "删除单条记录")) {
        printf("保存撤销快照失败，本次操作取消。\n");
        return;
    }
    if (delete_student_by_id(list, id)) {
        printf("删除成功。\n");
    } else {
        printf("删除失败。\n");
    }
}

static void delete_multiple_students(List *list, UndoStack *stack) {
    int count;
    int i;
    int deleted = 0;
    if (!read_int_range("请输入要批量删除的学号数量（1-100）：", 1, 100, &count)) {
        return;
    }
    if (!push_undo_snapshot(stack, list, "批量删除记录")) {
        printf("保存撤销快照失败，本次操作取消。\n");
        return;
    }
    for (i = 0; i < count; i++) {
        char id[ID_LEN];
        if (!read_nonempty_line("请输入学号：", id, ID_LEN)) {
            break;
        }
        if (delete_student_by_id(list, id)) {
            deleted++;
            printf("学号 %s 删除成功。\n", id);
        } else {
            printf("学号 %s 未找到，已跳过。\n", id);
        }
    }
    printf("批量删除完成，共删除 %d 条记录。\n", deleted);
}

static void query_students(const List *list) {
    int choice;
    Student *current;
    int found = 0;

    if (list->head == NULL) {
        printf("当前没有学生记录可查询。\n");
        return;
    }

    printf("请选择查询方式：\n");
    printf("1. 按学号精确查询\n");
    printf("2. 按姓名精确查询\n");
    printf("3. 按姓名模糊查询\n");
    printf("4. 按学号模糊查询\n");
    printf("5. 按单科成绩下限查询\n");
    printf("6. 按总分下限查询\n");
    if (!read_int_range("请输入选项：", 1, 6, &choice)) {
        return;
    }

    if (choice == 1) {
        char id[ID_LEN];
        if (!read_nonempty_line("请输入学号：", id, ID_LEN)) {
            return;
        }
        current = list->head;
        print_header();
        while (current != NULL) {
            if (strcmp(current->id, id) == 0) {
                print_student(current);
                found = 1;
            }
            current = current->next;
        }
    } else if (choice == 2) {
        char name[NAME_LEN];
        if (!read_nonempty_line("请输入姓名：", name, NAME_LEN)) {
            return;
        }
        current = list->head;
        print_header();
        while (current != NULL) {
            if (strcmp(current->name, name) == 0) {
                print_student(current);
                found = 1;
            }
            current = current->next;
        }
    } else if (choice == 3) {
        char keyword[NAME_LEN];
        if (!read_nonempty_line("请输入姓名关键字：", keyword, NAME_LEN)) {
            return;
        }
        current = list->head;
        print_header();
        while (current != NULL) {
            if (strstr(current->name, keyword) != NULL) {
                print_student(current);
                found = 1;
            }
            current = current->next;
        }
    } else if (choice == 4) {
        char keyword[ID_LEN];
        if (!read_nonempty_line("请输入学号关键字：", keyword, ID_LEN)) {
            return;
        }
        current = list->head;
        print_header();
        while (current != NULL) {
            if (strstr(current->id, keyword) != NULL) {
                print_student(current);
                found = 1;
            }
            current = current->next;
        }
    } else if (choice == 5) {
        int index;
        float score;
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "请输入课程编号（1-%d）：", COURSE_COUNT);
        if (!read_int_range(prompt, 1, COURSE_COUNT, &index)) {
            return;
        }
        if (!read_float_range("请输入最低成绩：", 0.0f, 100.0f, &score)) {
            return;
        }
        current = list->head;
        print_header();
        while (current != NULL) {
            if (current->scores[index - 1] >= score) {
                print_student(current);
                found = 1;
            }
            current = current->next;
        }
    } else if (choice == 6) {
        float total_limit;
        if (!read_float_range("请输入最低总分：", 0.0f, 500.0f, &total_limit)) {
            return;
        }
        current = list->head;
        print_header();
        while (current != NULL) {
            if (current->total >= total_limit) {
                print_student(current);
                found = 1;
            }
            current = current->next;
        }
    }

    if (!found) {
        printf("没有找到符合条件的记录。\n");
    }
}

static void split_list(Student *source, Student **front, Student **back) {
    Student *slow = source;
    Student *fast = source->next;
    while (fast != NULL) {
        fast = fast->next;
        if (fast != NULL) {
            slow = slow->next;
            fast = fast->next;
        }
    }
    *front = source;
    *back = slow->next;
    slow->next = NULL;
}

/* 归并排序适合链表，避免频繁随机访问，整体复杂度为 O(n log n)。 */
static Student *merge_sorted(Student *left, Student *right, int (*compare)(const Student *, const Student *)) {
    Student *result;
    if (left == NULL) {
        return right;
    }
    if (right == NULL) {
        return left;
    }
    if (compare(left, right) <= 0) {
        result = left;
        result->next = merge_sorted(left->next, right, compare);
    } else {
        result = right;
        result->next = merge_sorted(left, right->next, compare);
    }
    return result;
}

static void merge_sort(Student **head_ref, int (*compare)(const Student *, const Student *)) {
    Student *head = *head_ref;
    Student *front;
    Student *back;
    if (head == NULL || head->next == NULL) {
        return;
    }
    split_list(head, &front, &back);
    merge_sort(&front, compare);
    merge_sort(&back, compare);
    *head_ref = merge_sorted(front, back, compare);
}

static int compare_by_id(const Student *a, const Student *b) {
    return strcmp(a->id, b->id);
}

static int compare_by_total_desc(const Student *a, const Student *b) {
    if (a->total < b->total) {
        return 1;
    }
    if (a->total > b->total) {
        return -1;
    }
    return strcmp(a->id, b->id);
}

static int g_sort_course_index = 0;

static int compare_by_course_desc(const Student *a, const Student *b) {
    if (a->scores[g_sort_course_index] < b->scores[g_sort_course_index]) {
        return 1;
    }
    if (a->scores[g_sort_course_index] > b->scores[g_sort_course_index]) {
        return -1;
    }
    return strcmp(a->id, b->id);
}

static void sort_students(List *list, UndoStack *stack) {
    int choice;
    if (list->head == NULL || list->head->next == NULL) {
        printf("记录数量不足，无需排序。\n");
        return;
    }
    printf("请选择排序方式：\n");
    printf("1. 按学号升序\n");
    printf("2. 按总分降序\n");
    printf("3. 按单科成绩降序\n");
    if (!read_int_range("请输入选项：", 1, 3, &choice)) {
        return;
    }
    if (!push_undo_snapshot(stack, list, "排序")) {
        printf("保存撤销快照失败，本次操作取消。\n");
        return;
    }
    if (choice == 1) {
        merge_sort(&list->head, compare_by_id);
    } else if (choice == 2) {
        merge_sort(&list->head, compare_by_total_desc);
    } else {
        char prompt[64];
        int index;
        snprintf(prompt, sizeof(prompt), "请输入课程编号（1-%d）：", COURSE_COUNT);
        if (!read_int_range(prompt, 1, COURSE_COUNT, &index)) {
            return;
        }
        g_sort_course_index = index - 1;
        merge_sort(&list->head, compare_by_course_desc);
    }
    printf("排序完成。\n");
}

static void clear_all_students(List *list, UndoStack *stack) {
    int confirm;
    if (list->head == NULL) {
        printf("当前已经没有数据。\n");
        return;
    }
    if (!read_yes_no("确认清空全部记录吗？(y/n)：", &confirm)) {
        return;
    }
    if (!confirm) {
        printf("已取消清空操作。\n");
        return;
    }
    if (!push_undo_snapshot(stack, list, "清空全部记录")) {
        printf("保存撤销快照失败，本次操作取消。\n");
        return;
    }
    free_list(list);
    printf("已清空全部记录。\n");
}

static void show_menu(void) {
    printf("\n================ 学生成绩单管理系统 ================\n");
    printf("1. 从文件读取成绩单\n");
    printf("2. 保存成绩单到文件\n");
    printf("3. 显示全部学生记录\n");
    printf("4. 添加单个学生记录\n");
    printf("5. 批量添加学生记录\n");
    printf("6. 修改学生记录\n");
    printf("7. 删除单个学生记录\n");
    printf("8. 批量删除学生记录\n");
    printf("9. 条件查询学生记录\n");
    printf("10. 排序学生记录\n");
    printf("11. 撤销上一步操作\n");
    printf("12. 清空全部学生记录\n");
    printf("13. 查看文件格式说明\n");
    printf("0. 退出程序\n");
    printf("===================================================\n");
}

int main(void) {
    List list;
    UndoStack undo_stack;
    int choice;
    int save_before_exit;
    char path[PATH_LEN];

    init_list(&list);
    init_undo_stack(&undo_stack);

    printf("欢迎使用基于单向链表的学生成绩单管理系统。\n");
    print_format_tip();

    while (1) {
        show_menu();
        if (!read_int_range("请输入菜单编号：", 0, 13, &choice)) {
            continue;
        }
        if (choice == 1) {
            LoadReport report;
            if (!read_nonempty_line("请输入读取文件路径：", path, sizeof(path))) {
                continue;
            }
            if (!validate_path(path)) {
                continue;
            }
            if (!push_undo_snapshot(&undo_stack, &list, "从文件读取数据")) {
                printf("保存撤销快照失败，本次操作取消。\n");
                continue;
            }
            if (load_from_file(&list, path, &report)) {
                printf("文件读取完成，成功载入 %d 条记录，跳过重复记录 %d 条，无效记录 %d 条。\n",
                       report.loaded_count, report.duplicate_count, report.invalid_count);
            } else {
                printf("文件读取失败，请检查路径、权限或文件格式。\n");
                undo_last_action(&list, &undo_stack);
            }
        } else if (choice == 2) {
            if (!read_nonempty_line("请输入保存文件路径：", path, sizeof(path))) {
                continue;
            }
            if (!validate_path(path)) {
                continue;
            }
            if (save_to_file(&list, path)) {
                printf("文件保存成功。\n");
            } else {
                printf("文件保存失败，请检查路径和写入权限。\n");
            }
        } else if (choice == 3) {
            print_all_students(&list);
        } else if (choice == 4) {
            add_one_student(&list, &undo_stack);
        } else if (choice == 5) {
            add_multiple_students(&list, &undo_stack);
        } else if (choice == 6) {
            modify_student_info(&list, &undo_stack);
        } else if (choice == 7) {
            delete_one_student(&list, &undo_stack);
        } else if (choice == 8) {
            delete_multiple_students(&list, &undo_stack);
        } else if (choice == 9) {
            query_students(&list);
        } else if (choice == 10) {
            sort_students(&list, &undo_stack);
        } else if (choice == 11) {
            if (!undo_last_action(&list, &undo_stack)) {
                printf("当前没有可撤销的操作。\n");
            }
        } else if (choice == 12) {
            clear_all_students(&list, &undo_stack);
        } else if (choice == 13) {
            print_format_tip();
        } else if (choice == 0) {
            if (read_yes_no("退出前是否保存当前数据？(y/n)：", &save_before_exit) && save_before_exit) {
                if (read_nonempty_line("请输入保存文件路径：", path, sizeof(path)) && validate_path(path)) {
                    if (save_to_file(&list, path)) {
                        printf("文件保存成功，程序即将退出。\n");
                    } else {
                        printf("文件保存失败，程序仍将退出。\n");
                    }
                } else {
                    printf("保存路径无效，程序仍将退出。\n");
                }
            }
            break;
        }
    }

    free_list(&list);
    free_undo_stack(&undo_stack);
    printf("感谢使用，再见。\n");
    return 0;
}
