# 创建数据库
CREATE DATABASE IF NOT EXISTS xundao;

# 选择数据库
USE xundao;

SHOW TABLES; # 显示所有表

# 执行SQL语句(CREATE/ALTER TABLE)
DROP TABLE IF EXISTS emp;
DROP TABLE IF EXISTS emp1;
CREATE TABLE IF NOT EXISTS emp(ename VARCHAR(10), hiredate DATE, sal DECIMAL(10, 2), deptno INT(2));
DESC emp;
SHOW CREATE TABLE emp;

#select "=================================================================";
#ALTER TABLE emp MODIFY ename VARCHAR(32);               # 修改字段类型
#DESC emp;
#
#select "=================================================================";
#ALTER TABLE emp ADD age smallint;                       # 增加字段
#DESC emp;
#
#select "=================================================================";
#ALTER TABLE emp MODIFY age smallint after ename;        # 调整字段位置
#DESC emp;
#
#select "=================================================================";
#ALTER TABLE emp MODIFY age smallint first;              # 调整字段位置
#DESC emp;
#
#select "=================================================================";
#ALTER TABLE emp DROP age;                               # 删除字段
#DESC emp;
#
#select "=================================================================";
#ALTER TABLE emp RENAME emp1;                            # 重命名表
#SHOW CREATE TABLE emp1;
#
#select "=================================================================";
#ALTER TABLE emp1 RENAME emp;                            # 重命名表
#SHOW CREATE TABLE emp;

# 执行INSERT语句(INSERT INTO)
INSERT INTO emp(ename, hiredate, sal, deptno) VALUES('zz1', '2000-01-01', 2000, 1);
INSERT INTO emp(ename, hiredate, sal, deptno) VALUES('zz1', '2000-01-01', 2000, 1);
INSERT INTO emp(ename, hiredate, sal, deptno) VALUES('zz1', '2000-01-01', 2000, 1);
INSERT INTO emp(ename, hiredate, sal, deptno) VALUES('zz1', '2000-01-01', 2000, 1);
INSERT INTO emp(ename, hiredate, sal, deptno) VALUES('zz1', '2000-01-01', 2000, 1);
INSERT INTO emp VALUES('lisa', '2003-02-01', 3000, 2);
INSERT INTO emp VALUES('lisa', '2003-02-01', 3000, 2);
INSERT INTO emp VALUES('lisa', '2003-02-01', 3000, 2);
INSERT INTO emp VALUES('lisa', '2003-02-01', 3000, 2);
INSERT INTO emp VALUES('lisa', '2003-02-01', 3000, 2);
INSERT INTO emp VALUES('lisa', '2003-02-01', 3000, 2);
INSERT INTO emp VALUES('lisa', '2003-02-01', 3000, 2);
INSERT INTO emp VALUES('lisa', '2003-02-01', 3000, 2);
INSERT INTO emp VALUES('lisa', '2003-02-01', 3000, 2);
INSERT INTO emp(ename, sal) VALUES('dony', 1000);

DROP TABLE IF EXISTS dept;
CREATE TABLE IF NOT EXISTS dept(deptno INT(2) PRIMARY KEY, deptname VARCHAR(32) NOT NULL);

INSERT INTO dept VALUES(1, 'dept1'), (2, 'dept2');
INSERT INTO dept VALUES(3, 'dept3'), (4, 'dept4');

# 执行UPDATE语句
UPDATE emp SET sal=4000 WHERE ename='lisa';

SELECT * FROM emp; # 显示全部结果

#SELECT * FROM dept;
#SELECT DISTINCT * FROM emp; # 去重显示结果
#SELECT * FROM emp ORDER BY sal DESC, deptno ASC;
SELECT * FROM emp ORDER BY sal DESC, deptno ASC LIMIT 5;

SELECT ename, deptname FROM emp, dept WHERE emp.deptno = dept.deptno;
SELECT ename, deptname FROM emp LEFT JOIN dept ON emp.deptno = dept.deptno;
SELECT ename, deptname FROM emp RIGHT JOIN dept ON emp.deptno = dept.deptno;

# 创建视图VIEW
DROP VIEW IF EXISTS vw_test;

CREATE VIEW vw_test
AS
    SELECT id1 as id FROM t1;

# 创建存储过程PROCEDURE
DROP PROCEDURE IF EXISTS pr_add;
DROP PROCEDURE IF EXISTS pr_sub;
DROP PROCEDURE IF EXISTS pr_div;

DELIMITER // # 修改结束符
CREATE PROCEDURE pr_add(a int, b int)
BEGIN
    DECLARE c int;

    IF a IS NULL THEN
        SET a = 0;
    END IF;
    IF b IS NULL THEN
        SET b = 0;
    END IF;

    SET c = a + b;

    SELECT a, "+", b, "=", c as sum;
END//

CREATE PROCEDURE pr_sub(a int, b int)
BEGIN
    DECLARE c int;

    IF a IS NULL THEN
        SET a = 0;
    END IF;
    IF b IS NULL THEN
        SET b = 0;
    END IF;

    SET c = a - b;

    SELECT a, "-", b, "=", c as diff;
END//

CREATE PROCEDURE pr_div(a int, b int, OUT v int)
BEGIN
    IF a IS NULL THEN
        SET a = 0;
    END IF;
    IF b IS NULL THEN
        SET b = 1;
    END IF;

    SET v = a / b;

    SELECT a, "/", b, "=", v;
END//

DELIMITER ;

SET @a = 10;
SET @b = 20;
SET @c = 5;
SET @v = 1;

CALL pr_add(@a, @b);
CALL pr_add(@c, @b);
CALL pr_sub(@c, @b);
SELECT "Before call div: ", @v;
CALL pr_div(@b, @c, @v);
SELECT "After call div: ", @v;

# 创建触发器(TRIGGER)
DROP TRIGGER IF EXISTS tg_test;

DELIMITER //
CREATE TRIGGER tg_test
    AFTER INSERT ON emp
    FOR EACH ROW
    BEGIN
        INSERT INTO t1 VALUES(new.sal, new.deptno);
    END//
DELIMITER ;
