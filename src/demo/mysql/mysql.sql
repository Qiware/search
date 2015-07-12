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

# 
SELECT ename, deptname FROM emp, dept WHERE emp.deptno = dept.deptno;
SELECT ename, deptname FROM emp LEFT JOIN dept ON emp.deptno = dept.deptno;
SELECT ename, deptname FROM emp RIGHT JOIN dept ON emp.deptno = dept.deptno;
