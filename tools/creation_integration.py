"""Exercise creation in a disposable catalog copied from the original SQL schema/procedures."""
import secrets
import re
import subprocess

from database import identifier, literal, sql


def run(command, build_directory="build"):
    name = "Eternity_Test_Creation_" + secrets.token_hex(6)
    catalog = identifier(name)
    collation = sql("SELECT CONVERT(varchar(128),DATABASEPROPERTYEX('PS_GameData','Collation'));")
    if not re.fullmatch(r"[A-Za-z0-9_]+", collation):
        raise ValueError("Invalid source database collation identifier")
    sql(f"CREATE DATABASE {catalog} COLLATE {collation};")
    try:
        tables = ("Chars", "CharItems", "UserMaxGrow", "GuildChars", "_DeletedChars")
        for table in tables:
            sql(f"SELECT TOP (0) * INTO dbo.{identifier(table)} FROM PS_GameData.dbo.{identifier(table)};", name)
        defaults = sql("""SELECT t.name AS [table],c.name AS [column],d.definition
            FROM sys.default_constraints d JOIN sys.tables t ON t.object_id=d.parent_object_id
            JOIN sys.columns c ON c.object_id=t.object_id AND c.column_id=d.parent_column_id
            WHERE t.name IN ('Chars','CharItems','UserMaxGrow','GuildChars','_DeletedChars') FOR JSON PATH;""", "PS_GameData", json_output=True)
        for default in defaults:
            sql(f"ALTER TABLE dbo.{identifier(default['table'])} ADD DEFAULT {default['definition']} "
                f"FOR {identifier(default['column'])};", name)
        modules = sql("""SELECT m.definition FROM sys.sql_modules m
            JOIN sys.procedures p ON p.object_id=m.object_id
            WHERE p.name IN ('usp_Create_Char_R','usp_Save_Char_Item_Add_E','usp_Save_Char_Country_E',
                'usp_Read_Chars_R','usp_Read_Char_Items_Simple_R','usp_Read_User_MaxGrow_R','usp_Delete_Char_E')
            FOR JSON PATH;""", "PS_GameData", json_output=True)
        if len(modules) != 7:
            raise ValueError("Original creation procedure set is incomplete")
        for module in modules:
            sql(f"EXEC({literal(module['definition'])});", name)
        # A failure on the second starter item proves the outer character transaction rolls back.
        sql("ALTER TABLE dbo.CharItems ADD CONSTRAINT EternityRejectFixtureItem CHECK(ItemID<>2147483000); "
            "CREATE USER [eternity] FOR LOGIN [eternity]; "
            "GRANT SELECT,INSERT,UPDATE,DELETE,EXECUTE ON SCHEMA::dbo TO [eternity];", name)
        subprocess.run(command + [f"{build_directory}/creation-sql-integration", name], check=True)
        subprocess.run(command + [f"{build_directory}/creation-network-integration", name], check=True)
    finally:
        # Only the fresh, randomly named fixture catalog is removed.
        sql(f"ALTER DATABASE {catalog} SET SINGLE_USER WITH ROLLBACK IMMEDIATE; DROP DATABASE {catalog};")
