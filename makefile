CC = gcc
OBJ = FileSystem.o main.o

EXE = run

# 編譯規則：編譯 .c 檔案為 .o 檔案
.c.o: 
	$(CC) -c $*.c

# 目標程式的建立，鏈接時加上 -lc 來鏈接標準 C 庫
$(EXE): $(OBJ)
	$(CC) -o $@ $(OBJ) 

# 清理目標，移除執行檔、物件檔案等
clean:
	rm -rf $(EXE) *.o *.d core
